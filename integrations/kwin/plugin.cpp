#include <plugin.h>
#include <input.h>
#include <input_event.h>
#include <pointer_input.h>
#include <wayland_server.h>
#include <wayland/seat.h>
#include <abstract_data_source.h>
#include <main.h>
#include <scene/workspacescene.h>
#include <scene/item.h>
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
#include <QSettings>
#include <fcntl.h>
#include <unistd.h>
#include "../../desktop/radialwheel.h"

namespace {
QString executable(const QString &name) {
    // Isolated tests point at freshly built binaries; desktop sessions leave it unset.
    if (const auto dir=qEnvironmentVariable("LORENZ_BIN_DIR"); !dir.isEmpty()) return dir+"/"+name;
    const auto local=QDir::homePath()+"/.local/bin/"+name;
    if (QFileInfo(local).isExecutable()) return local;
    return QStandardPaths::findExecutable(name);
}
// Wheel keys, shared with the companion's settings. Read at each gesture so a
// change applies to the next drag without reloading the extension.
struct WheelKey { Qt::Key key; Qt::KeyboardModifier modifier; QString label; };
QVector<WheelKey> wheelKeys() {
    QSettings settings(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)+"/lorenz/lorenz.conf",QSettings::IniFormat);
    const QStringList names{"Convert","Tools","Actions"}, defaults{"Shift","Control","Alt"};
    QVector<WheelKey> keys;
    for (int i=0;i<3;++i) {
        const auto name=settings.value("Wheels/"+names[i],defaults[i]).toString();
        if (name=="Shift") keys.append({Qt::Key_Shift,Qt::ShiftModifier,"Shift"});
        else if (name=="Control") keys.append({Qt::Key_Control,Qt::ControlModifier,"Ctrl"});
        else if (name=="Alt") keys.append({Qt::Key_Alt,Qt::AltModifier,"Alt"});
        else keys.append({Qt::Key_unknown,Qt::NoModifier,"—"});
    }
    return keys;
}
const QStringList kinds{"convert","tools","actions"};
// KWin paints the dragged file's icon above every window, so it would cover
// the lower sectors. Find it by class name: the type is not exported.
KWin::Item *dragIconItem(KWin::Item *item) {
    if (!item) return nullptr;
    if (qstrcmp(item->metaObject()->className(),"KWin::DragAndDropIconItem")==0) return item;
    for (auto *child:item->childItems()) if (auto *found=dragIconItem(child)) return found;
    return nullptr;
}
}

class LorenzPlugin : public KWin::Plugin, public KWin::InputEventFilter {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.lorenz.KWin")
    RadialWheel wheel;
    QTimer deadline;
    QProcess probe;
    QPointer<KWin::AbstractDataSource> source;
    QPointer<KWin::Item> hiddenIcon;
    QSocketNotifier *reader=nullptr;
    int readFd=-1;
    bool active=false;
    bool ending=false;
    QByteArray payload;
    QStringList files;
    QVector<WheelKey> keys=wheelKeys();
    RadialWheel::Mode mode=RadialWheel::Convert;
public:
    LorenzPlugin() : InputEventFilter(KWin::InputFilterOrder::ScreenEdge) {
        // Runs after the lock-screen filter but before normal drag-and-drop.
        KWin::input()->installInputEventFilter(this);
        deadline.setSingleShot(true); deadline.setInterval(5000);
        connect(&deadline,&QTimer::timeout,this,[this]{ abort(); });
        connect(KWin::waylandServer()->seat(),&KWin::SeatInterface::dragEnded,this,[this]{ if (!ending) reset(); });
        connect(&probe,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus) {
            if (!active) return;
            deadline.stop();
            if (code!=0) { wheel.message="Engine unavailable"; wheel.setItems({}); return; }
            QVector<RadialWheel::Item> items;
            for (const auto entry:QJsonDocument::fromJson(probe.readAllStandardOutput()).array()) {
                const auto item=entry.toObject();
                items.append({item["id"].toString(),item["label"].toString(),item["glyph"].toString()});
            }
            wheel.message=mode==RadialWheel::Convert?"No common format":"Nothing available";
            wheel.setItems(items);
        });
        connect(&probe,&QProcess::errorOccurred,this,[this](QProcess::ProcessError e){ if (e==QProcess::FailedToStart) abort(); });
        QDBusConnection::sessionBus().registerObject("/Lorenz",this,QDBusConnection::ExportAllSlots|QDBusConnection::ExportAllProperties);
    }
    ~LorenzPlugin() override {
        QDBusConnection::sessionBus().unregisterObject("/Lorenz");
        abort();
    }
    Q_PROPERTY(QString BuildId READ buildId)
    Q_PROPERTY(int WheelSize READ wheelSize)
    Q_INVOKABLE QString buildId() const { return QStringLiteral(LORENZ_PLUGIN_ID); }
    Q_INVOKABLE int wheelSize() const { return wheel.width(); }
    Q_PROPERTY(bool Active READ isActive)
    Q_PROPERTY(QStringList Formats READ formatList)
    Q_PROPERTY(QPoint WheelPosition READ wheelPosition)
    Q_PROPERTY(QString Mode READ modeName)
    Q_PROPERTY(bool DragIconHidden READ dragIconHidden)
    Q_INVOKABLE bool dragIconHidden() const { return hiddenIcon && !hiddenIcon->isVisible(); }
    Q_INVOKABLE QString modeName() const { return kinds[mode]; }
    Q_INVOKABLE bool isActive() const { return active; }
    Q_INVOKABLE QStringList formatList() const { return wheel.formats; }
    Q_INVOKABLE QPoint wheelPosition() const { return wheel.pos(); }
    Q_INVOKABLE QString BuildId() const { return buildId(); }
    Q_INVOKABLE int WheelSize() const { return wheelSize(); }
    Q_INVOKABLE bool Active() const { return isActive(); }
    Q_INVOKABLE QStringList Formats() const { return formatList(); }
    Q_INVOKABLE QPoint WheelPosition() const { return wheelPosition(); }
    Q_INVOKABLE QString Mode() const { return modeName(); }
    Q_INVOKABLE bool DragIconHidden() const { return dragIconHidden(); }
    bool pointerMotion(KWin::PointerMotionEvent *event) override {
        if (!active) return false;
        wheel.trackGlobal(event->position); return true;
    }
    // Wheels open on a key *press* during a drag, never because a modifier was
    // already held: Ctrl-drag and Shift-drag keep their usual copy/move meaning.
    bool keyboardKey(KWin::KeyboardKeyEvent *event) override {
        const bool pressed=event->state==KWin::KeyboardKeyState::Pressed;
        if (event->key==Qt::Key_Shift || event->key==Qt::Key_Control || event->key==Qt::Key_Alt) {
            if (!pressed) return false;
            if (!active && KWin::waylandServer()->seat()->isDragPointer()) keys=wheelKeys();
            for (int i=0;i<3;++i) if (event->key==keys[i].key) {
                // The Wayland seat position can stop updating during drag-and-drop.
                // Read the compositor pointer at key press, not the drag's origin.
                if (!active) maybeActivate(RadialWheel::Mode(i),KWin::input()->pointer()->pos());
                else if (mode!=i) switchTo(RadialWheel::Mode(i));
            }
            return false;
        }
        if (!active) return false;
        if (pressed) switch (event->key) {
        case Qt::Key_Escape: abort(); break;
        case Qt::Key_Left: case Qt::Key_Up: wheel.step(-1); break;
        case Qt::Key_Right: case Qt::Key_Down: case Qt::Key_Tab: wheel.step(1); break;
        case Qt::Key_Return: case Qt::Key_Enter: launch(); break;
        default: break;
        }
        // While the wheel is open, other keys belong to it, not to a drop target.
        return true;
    }
    bool pointerButton(KWin::PointerButtonEvent *event) override {
        if (!active) return false;
        if (event->button==Qt::LeftButton && event->state==KWin::PointerButtonState::Released) {
            wheel.trackGlobal(event->position);
            launch();
            auto *seat=KWin::waylandServer()->seat();
            seat->notifyPointerButton(event->nativeButton,event->state); seat->notifyPointerFrame();
        }
        return true;
    }
private:
    void maybeActivate(RadialWheel::Mode wanted,QPointF pos) {
        auto *seat=KWin::waylandServer()->seat();
        if (active || probe.state()!=QProcess::NotRunning || !seat->isDragPointer()) return;
        if (!KWin::input()->pointer()->buttons().testFlag(Qt::LeftButton)) return;
        auto *candidate=seat->dragSource();
        if (!candidate || !candidate->mimeTypes().contains("text/uri-list")) return;
        if (executable("lorenz").isEmpty() || executable("lorenz-desktop").isEmpty()) return;
        int pipeFd[2]; if (pipe2(pipeFd,O_CLOEXEC)!=0) return;
        fcntl(pipeFd[0],F_SETFL,O_NONBLOCK);
        source=candidate; active=true; payload.clear(); files.clear(); mode=wanted;
        QStringList labels; for (const auto &k:keys) labels << k.label; wheel.tabKeys=labels;
        wheel.mode=mode; wheel.setItems({}); wheel.setLoading(true); wheel.sourceName.clear(); wheel.sourceType.clear();
        wheel.message="Reading files…"; wheel.openAt(pos.toPoint()); hideDragIcon();
        seat->setDragTarget(nullptr,nullptr,pos,QMatrix4x4());
        readFd=pipeFd[0]; reader=new QSocketNotifier(readFd,QSocketNotifier::Read,this);
        connect(reader,&QSocketNotifier::activated,this,[this]{ readUris(); });
        candidate->requestData("text/uri-list",KWin::FileDescriptor(pipeFd[1]));
        deadline.start();
    }
    // Another wheel key while open: same files, new items, same position.
    void switchTo(RadialWheel::Mode wanted) {
        mode=wanted; wheel.setMode(mode);
        if (files.isEmpty()) return; // Still reading; the query will use the new mode.
        if (probe.state()!=QProcess::NotRunning) { probe.kill(); probe.waitForFinished(500); }
        wheel.setItems({}); wheel.setLoading(true); query();
    }
    // Cancel the source drag, rather than completing Shift's move action. It
    // therefore cannot delete or move the original files.
    void launch() {
        const QString id=wheel.selected(); const QStringList paths=files; const auto chosen=mode;
        abort();
        if (id.isEmpty() || paths.isEmpty()) return;
        static const QStringList flags{"--convert-to","--tool","--action"};
        QProcess::startDetached(executable("lorenz-desktop"),QStringList{flags[chosen],id,"--"}+paths);
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
                    if (!info.exists()) { abort(); return; }
                    const auto path=info.canonicalFilePath(); if (!files.contains(path)) files << path;
                    if (files.size()>128) { abort(); return; }
                }
                if (files.isEmpty()) { abort(); return; }
                const QFileInfo first(files.first());
                wheel.sourceName=files.size()==1?first.fileName():QString("%1 items").arg(files.size());
                wheel.sourceType=first.isDir()?"folder":first.suffix().toLower();
                query(); return;
            } else {
                if (errno!=EAGAIN && errno!=EINTR) abort();
                return;
            }
        }
    }
    void query() { deadline.start(); probe.start(executable("lorenz"),QStringList{"menu",kinds[mode],"--json","--"}+files); }
    void hideDragIcon() {
        auto *scene=KWin::kwinApp()->scene(); if (!scene) return;
        auto *icon=dragIconItem(scene->overlayItem()); if (!icon) icon=dragIconItem(scene->containerItem());
        if (icon) { icon->setVisible(false); hiddenIcon=icon; }
    }
    void reset() {
        if (hiddenIcon) hiddenIcon->setVisible(true);
        hiddenIcon.clear();
        active=false; deadline.stop(); closePipe(); wheel.dismiss(); source.clear();
        if (probe.state()!=QProcess::NotRunning) probe.kill();
    }
    void abort() {
        const bool owned=active;
        reset();
        if (owned) { ending=true; KWin::waylandServer()->seat()->cancelDrag(); ending=false; }
    }
};

class LorenzFactory : public KWin::PluginFactory {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID PluginFactory_iid FILE "lorenz-metadata.json")
    Q_INTERFACES(KWin::PluginFactory)
public:
    std::unique_ptr<KWin::Plugin> create() const override { return std::make_unique<LorenzPlugin>(); }
};
#include "plugin.moc"
