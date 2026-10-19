#pragma once
// Windows drag gesture: drag files, tap Shift, Ctrl or Alt, release over a
// sector. Windows gives the dragged files only to drop targets, so the wheel
// is one: it opens under the pointer and reads the files when the drag enters
// it. A mouse hook on its own thread reports left-button presses; modifiers
// are polled only while the button is held, and a modifier held before the
// press keeps its usual copy/move meaning. Typed text is never read.
#include "dropwheel.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <array>
#include <thread>

class WindowsGesture : public QObject {
public:
    using Launch = std::function<void(RadialWheel::Mode, QString, QStringList)>;
    // keys(): the configured key of each wheel ("Shift", "Control", "Alt" or
    // "None"), read at every press like the KWin extension does.
    WindowsGesture(QString core, std::function<QStringList()> keys, std::function<QString(QString)> label, Launch launch)
        : core(std::move(core)), keys(std::move(keys)), label(std::move(label)), launch(std::move(launch)) {
        self = this;
        wheel.message = "Nothing available";
        wheel.modeRequested = [this](RadialWheel::Mode mode) { load(mode); };
        wheel.filesEntered = [this](const QStringList &entered) {
            if (entered == files) return;
            files = entered;
            wheel.sourceName = files.size() == 1 ? QFileInfo(files.first()).fileName() : QString("%1 items").arg(files.size());
            wheel.sourceType = QFileInfo(files.first()).isDir() ? "folder" : QFileInfo(files.first()).suffix().toLower();
            load(wheel.mode, true);
        };
        // A drop during a wheel switch could hit an item of the previous wheel.
        wheel.droppedOn = [this](const QString &id) { dropped = true; if (shown == wheel.mode) this->launch(wheel.mode, id, files); };
        poll.setInterval(25);
        connect(&poll, &QTimer::timeout, this, [this] { tick(); });
        hook = std::thread([] {
            HHOOK handle = SetWindowsHookExW(WH_MOUSE_LL, mouseProc, GetModuleHandleW(nullptr), 0);
            MSG message;
            while (GetMessageW(&message, nullptr, 0, 0) > 0) {}
            if (handle) UnhookWindowsHookEx(handle);
        });
    }
    ~WindowsGesture() override {
        const DWORD thread = GetThreadId(hook.native_handle());
        // The queue exists once the thread has installed its hook.
        for (int i = 0; i < 50 && !PostThreadMessageW(thread, WM_QUIT, 0, 0); ++i) Sleep(10);
        hook.join();
        self = nullptr;
    }
private:
    static inline WindowsGesture *self = nullptr;
    static LRESULT CALLBACK mouseProc(int code, WPARAM message, LPARAM data) {
        if (code == HC_ACTION && (message == WM_LBUTTONDOWN || message == WM_LBUTTONUP)) {
            const bool down = message == WM_LBUTTONDOWN;
            // Return at once: the system waits for this hook on every click.
            QMetaObject::invokeMethod(qApp, [down] { if (self) self->button(down); }, Qt::QueuedConnection);
        }
        return CallNextHookEx(nullptr, code, message, data);
    }
    static std::array<bool, 3> modifiers() {
        return {GetAsyncKeyState(VK_SHIFT) < 0, GetAsyncKeyState(VK_CONTROL) < 0, GetAsyncKeyState(VK_MENU) < 0};
    }
    void button(bool down) {
        if (down) {
            held = modifiers();
            poll.start();
            return;
        }
        poll.stop();
        // The drop reaches the wheel just after the release; give it time.
        if (wheel.isVisible()) QTimer::singleShot(400, &wheel, [this] { if (!dropped) wheel.dismiss(); });
    }
    void tick() {
        if (wheel.isVisible() && GetAsyncKeyState(VK_ESCAPE) < 0) { wheel.dismiss(); return; }
        static const QStringList names{"Shift", "Control", "Alt"};
        const auto now = modifiers();
        for (int i = 0; i < 3; ++i) {
            if (now[i] && !held[i]) pressed(names[i]);
        }
        held = now;
    }
    void pressed(const QString &key) {
        const QStringList current = keys();
        const int mode = current.indexOf(key);
        if (mode < 0) return;
        if (wheel.isVisible()) { load(RadialWheel::Mode(mode)); return; }
        // Moving or resizing a window also holds the button: not a file drag.
        GUITHREADINFO info{sizeof(info)};
        if (GetGUIThreadInfo(0, &info) && (info.flags & GUI_INMOVESIZE)) return;
        files.clear(); dropped = false; shown = -1;
        wheel.tabKeys.clear();
        for (const QString &k : current) wheel.tabKeys << label(k);
        wheel.sourceName.clear(); wheel.sourceType.clear();
        wheel.mode = RadialWheel::Mode(mode);
        wheel.setItems({});
        wheel.setLoading(true);
        wheel.openAt(QCursor::pos());
    }
    // Items come from `metroid menu`, read without blocking the wheel.
    void load(RadialWheel::Mode mode, bool force = false) {
        if (mode == wheel.mode && !force) return;
        wheel.setMode(mode);
        if (files.isEmpty()) return;
        static const QStringList kinds{"convert", "tools", "actions"};
        if (menu) { menu->disconnect(); menu->kill(); menu->deleteLater(); }
        menu = new QProcess(this);
        wheel.setLoading(true); shown = -1;
        connect(menu, &QProcess::finished, this, [this, process = menu, mode] {
            QVector<RadialWheel::Item> items;
            for (const auto entry : QJsonDocument::fromJson(process->readAllStandardOutput()).array()) {
                const auto item = entry.toObject();
                items.append({item["id"].toString(), item["label"].toString(), item["glyph"].toString()});
            }
            process->deleteLater();
            if (process == menu) menu = nullptr;
            if (wheel.mode == mode) { wheel.setItems(items); shown = mode; }
        });
        menu->start(core, QStringList{"menu", kinds[mode], "--json", "--"} + files);
    }
    QString core;
    std::function<QStringList()> keys;
    std::function<QString(QString)> label;
    Launch launch;
    DropWheel wheel;
    QTimer poll;
    QProcess *menu = nullptr;
    std::array<bool, 3> held{};
    QStringList files;
    bool dropped = false;
    int shown = -1; // the wheel whose items are on screen
    std::thread hook;
};
