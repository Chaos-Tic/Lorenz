#include <plugin.h>
#include <input.h>
#include <input_event.h>
#include <pointer_input.h>
#include <wayland_server.h>
#include <wayland/seat.h>
#include <abstract_data_source.h>
#include <utils/filedescriptor.h>
#include <QSocketNotifier>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <QPointer>
#include <QStandardPaths>
#include <QDBusConnection>
#include <fcntl.h>
#include <unistd.h>
#include "../../desktop/radialwheel.h"

namespace {
QString executable(const QString &name) {
    const auto local=QDir::homePath()+"/.local/bin/"+name;
    if (QFileInfo(local).isExecutable()) return local;
    return QStandardPaths::findExecutable(name);
}
}

class MetroidPlugin : public KWin::Plugin, public KWin::InputEventFilter {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.metroid.KWin")
    RadialWheel wheel;
    QTimer deadline;
    QProcess probe;
    QPointer<KWin::AbstractDataSource> source;
    QSocketNotifier *reader=nullptr;
    int readFd=-1;
    bool active=false;
    bool ending=false;
    QByteArray payload;
    QStringList files;
    QSet<QString> common;
    int queryIndex=0;
public:
    MetroidPlugin() : InputEventFilter(KWin::InputFilterOrder::ScreenEdge) {
        // Runs after the lock-screen filter but before normal drag-and-drop.
        KWin::input()->installInputEventFilter(this);
        deadline.setSingleShot(true); deadline.setInterval(5000);
        connect(&deadline,&QTimer::timeout,this,[this]{ abort(); });
        connect(KWin::waylandServer()->seat(),&KWin::SeatInterface::dragEnded,this,[this]{ if (!ending) reset(); });
        connect(&probe,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus) {
            if (!active) return;
            if (code!=0) { wheel.message="Engine unavailable\nRelease to cancel"; wheel.update(); return; }
            QSet<QString> supported;
            for (const auto entry:QJsonDocument::fromJson(probe.readAllStandardOutput()).array()) {
                const auto f=entry.toObject(); if (f["available"].toBool()) supported.insert(f["target"].toString());
            }
            if (queryIndex==0) common=supported; else common.intersect(supported);
            ++queryIndex;
            if (queryIndex<files.size()) query();
            else {
                deadline.stop(); wheel.formats=common.values(); wheel.formats.sort();
                wheel.message="No common format\nRelease to cancel"; wheel.update();
            }
        });
        connect(&probe,&QProcess::errorOccurred,this,[this](QProcess::ProcessError e){ if (e==QProcess::FailedToStart) abort(); });
        QDBusConnection::sessionBus().registerObject("/Metroid",this,QDBusConnection::ExportAllSlots|QDBusConnection::ExportAllProperties);
    }
    ~MetroidPlugin() override {
        QDBusConnection::sessionBus().unregisterObject("/Metroid");
        abort();
    }
    Q_PROPERTY(QString BuildId READ buildId)
    Q_PROPERTY(int WheelSize READ wheelSize)
    QString buildId() const { return QStringLiteral(METROID_PLUGIN_ID); }
    int wheelSize() const { return wheel.width(); }
    Q_PROPERTY(bool Active READ isActive)
    Q_PROPERTY(QStringList Formats READ formatList)
    Q_PROPERTY(QPoint WheelPosition READ wheelPosition)
    bool isActive() const { return active; }
    QStringList formatList() const { return wheel.formats; }
    QPoint wheelPosition() const { return wheel.pos(); }
    bool pointerMotion(KWin::PointerMotionEvent *event) override {
        maybeActivate(event->modifiers,event->position);
        if (!active) return false;
        wheel.trackGlobal(event->position); return true;
    }
    bool keyboardKey(KWin::KeyboardKeyEvent *event) override {
        if (event->key==Qt::Key_Shift && event->state==KWin::KeyboardKeyState::Pressed)
            maybeActivate(Qt::ShiftModifier,KWin::waylandServer()->seat()->pointerPos());
        if (active && event->key==Qt::Key_Escape && event->state==KWin::KeyboardKeyState::Pressed) { abort(); return true; }
        return false;
    }
    bool pointerButton(KWin::PointerButtonEvent *event) override {
        if (!active) return false;
        if (event->button==Qt::LeftButton && event->state==KWin::PointerButtonState::Released) {
            wheel.trackGlobal(event->position);
            const QString target=wheel.selected(); const QStringList paths=files;
            // Cancel the source drag, rather than completing Shift's move action.
            // It therefore cannot delete or move the original files.
            abort();
            auto *seat=KWin::waylandServer()->seat();
            seat->notifyPointerButton(event->nativeButton,event->state); seat->notifyPointerFrame();
            if (!target.isEmpty() && !paths.isEmpty()) {
                QStringList args{"--convert-to",target}; args.append(paths);
                QProcess::startDetached(executable("metroid-desktop"),args);
            }
        }
        return true;
    }
private:
    void maybeActivate(Qt::KeyboardModifiers modifiers,QPointF pos) {
        auto *seat=KWin::waylandServer()->seat();
        if (active || probe.state()!=QProcess::NotRunning || !modifiers.testFlag(Qt::ShiftModifier) || !seat->isDragPointer()) return;
        if (!KWin::input()->pointer()->buttons().testFlag(Qt::LeftButton)) return;
        auto *candidate=seat->dragSource();
        if (!candidate || !candidate->mimeTypes().contains("text/uri-list")) return;
        if (executable("metroid").isEmpty() || executable("metroid-desktop").isEmpty()) return;
        int pipeFd[2]; if (pipe2(pipeFd,O_CLOEXEC)!=0) return;
        fcntl(pipeFd[0],F_SETFL,O_NONBLOCK);
        source=candidate; active=true; payload.clear(); files.clear(); common.clear(); queryIndex=0;
        wheel.formats.clear(); wheel.message="Reading files…"; wheel.openAt(pos.toPoint());
        seat->setDragTarget(nullptr,nullptr,pos,QMatrix4x4());
        readFd=pipeFd[0]; reader=new QSocketNotifier(readFd,QSocketNotifier::Read,this);
        connect(reader,&QSocketNotifier::activated,this,[this]{ readUris(); });
        candidate->requestData("text/uri-list",KWin::FileDescriptor(pipeFd[1]));
        deadline.start();
    }
    void closePipe() {
        if (reader) { reader->setEnabled(false); reader->deleteLater(); reader=nullptr; }
        if (readFd>=0) { ::close(readFd); readFd=-1; }
    }
    void readUris() {
        char buffer[8192];
        while (active && readFd>=0) {
            const auto count=::read(readFd,buffer,sizeof(buffer));
            if (count>0) {
                payload.append(buffer,count);
                if (payload.size()>262144) { abort(); return; }
            } else if (count==0) {
                closePipe();
                for (const auto &line:payload.split('\n')) {
                    const auto trimmed=line.trimmed(); if (trimmed.isEmpty() || trimmed.startsWith('#')) continue;
                    const auto url=QUrl::fromEncoded(trimmed);
                    if (!url.isLocalFile() || (!url.host().isEmpty() && url.host()!="localhost")) { abort(); return; }
                    const QFileInfo info(url.toLocalFile());
                    if (!info.isFile()) { abort(); return; }
                    const auto path=info.canonicalFilePath(); if (!files.contains(path)) files << path;
                    if (files.size()>128) { abort(); return; }
                }
                if (files.isEmpty()) { abort(); return; }
                query(); return;
            } else {
                if (errno!=EAGAIN && errno!=EINTR) abort();
                return;
            }
        }
    }
    void query() { probe.start(executable("metroid"),{"formats",files[queryIndex],"--json"}); }
    void reset() {
        active=false; deadline.stop(); closePipe(); wheel.hide(); source.clear();
        if (probe.state()!=QProcess::NotRunning) probe.kill();
    }
    void abort() {
        const bool owned=active;
        reset();
        if (owned) { ending=true; KWin::waylandServer()->seat()->cancelDrag(); ending=false; }
    }
};

class MetroidFactory : public KWin::PluginFactory {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID PluginFactory_iid FILE "metroid-metadata.json")
    Q_INTERFACES(KWin::PluginFactory)
public:
    std::unique_ptr<KWin::Plugin> create() const override { return std::make_unique<MetroidPlugin>(); }
};
#include "plugin.moc"
