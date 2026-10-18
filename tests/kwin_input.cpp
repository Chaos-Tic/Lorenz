// Test-only input driver. Never install this module in a normal desktop session.
#include <plugin.h>
#include <input.h>
#include <pointer_input.h>
#include <keyboard_input.h>
#include <wayland_server.h>
#include <wayland/seat.h>
#include <QDBusConnection>
#include <QApplication>
#include <QWidget>
#include <QTimer>
#include <QElapsedTimer>
class TestInput : public KWin::Plugin {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.metroid.TestInput")
    QElapsedTimer time;
public:
    TestInput() {
        time.start();
        QDBusConnection::sessionBus().registerObject("/MetroidTestInput",this,QDBusConnection::ExportAllSlots);
        QTimer::singleShot(1000,this,[]{ KWin::waylandServer()->seat()->setHasPointer(true); KWin::waylandServer()->seat()->setHasKeyboard(true); });
    }
public Q_SLOTS:
    void Move(int x,int y) { KWin::input()->pointer()->processMotionAbsolute(QPointF(x,y),std::chrono::microseconds(time.nsecsElapsed()/1000)); KWin::waylandServer()->seat()->notifyPointerFrame(); }
    void Button(bool down) { KWin::input()->pointer()->processButton(272,down?KWin::PointerButtonState::Pressed:KWin::PointerButtonState::Released,std::chrono::microseconds(time.nsecsElapsed()/1000)); KWin::waylandServer()->seat()->notifyPointerFrame(); }
    void Key(int key,bool down) { KWin::input()->keyboard()->processKey(key,down?KWin::KeyboardKeyState::Pressed:KWin::KeyboardKeyState::Released,std::chrono::microseconds(time.nsecsElapsed()/1000)); }
    bool Dragging() { return KWin::waylandServer()->seat()->isDragPointer(); }
    bool WheelCenteredAt(int x,int y) {
        for (auto *w:QApplication::topLevelWidgets()) if (w->objectName()=="metroid-wheel")
            return w->mapToGlobal(QPoint(w->width()/2,w->height()/2))==QPoint(x,y);
        return false;
    }
    bool Snapshot(QString path) { for (auto *w:QApplication::topLevelWidgets()) if (w->objectName()=="metroid-wheel") return w->grab().save(path); return false; }
};
class TestFactory : public KWin::PluginFactory {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID PluginFactory_iid FILE "kwin_input.json")
    Q_INTERFACES(KWin::PluginFactory)
public:
    std::unique_ptr<KWin::Plugin> create() const override {
        if (qEnvironmentVariable("METROID_ISOLATED_TEST")!="1") return nullptr;
        return std::make_unique<TestInput>();
    }
};
#include "kwin_input.moc"
