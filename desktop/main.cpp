#include <QtWidgets>
#include <cmath>
#include "radialwheel.h"
#ifdef Q_OS_WIN
#include "windowsgesture.h"
#endif

static QString corePath() {
    QString name = "lorenz";
#ifdef Q_OS_WIN
    name += ".exe";
#endif
    const QString sibling = QCoreApplication::applicationDirPath() + "/" + name;
    if (QFile::exists(sibling)) return sibling;
    return QStandardPaths::findExecutable(name);
}
static QJsonDocument query(const QStringList &args) {
    QProcess p;
    p.start(corePath(), args);
    if (!p.waitForStarted(2000) || !p.waitForFinished(5000)) return {};
    return QJsonDocument::fromJson(p.readAllStandardOutput());
}
static QLabel *label(const QString &text, const char *name = "") {
    auto *l = new QLabel(text); l->setObjectName(name); l->setTextFormat(Qt::PlainText); l->setWordWrap(true); return l;
}

class DropZone : public QFrame {
public:
    std::function<void(QStringList)> receive;
    explicit DropZone(QWidget *parent = nullptr) : QFrame(parent) {
        setAcceptDrops(true); setObjectName("dropZone"); setMinimumHeight(160);
        auto *layout = new QVBoxLayout(this);
        auto *symbol = label("＋", "dropSymbol"); symbol->setAlignment(Qt::AlignCenter);
        auto *title = label(L("Drop your files here"), "dropTitle"); title->setAlignment(Qt::AlignCenter);
        auto *hint = label(L("Images, audio, video, documents and PDF"), "muted"); hint->setAlignment(Qt::AlignCenter);
        layout->addWidget(symbol); layout->addWidget(title); layout->addWidget(hint); layout->addSpacing(10);
    }
    void dragEnterEvent(QDragEnterEvent *e) override { if (e->mimeData()->hasUrls()) e->acceptProposedAction(); }
    void dropEvent(QDropEvent *e) override {
        QStringList files;
        for (const auto &url : e->mimeData()->urls()) if (url.isLocalFile()) files << url.toLocalFile();
        if (receive) receive(files); e->acceptProposedAction();
    }
};

class Window : public QMainWindow {
    QTableWidget *table;
    QComboBox *format;
    QLabel *summary, *destination, *status;
    QPushButton *start, *stop, *clear, *browse, *choose;
    QProgressBar *progress;
    QProcess worker;
    QTemporaryDir cancellation;
    QString outputDir, lastOutput;
    int current = -1, succeeded = 0, failed = 0;
    bool cancelling = false;
    QByteArray response;
public:
    Window() {
        setWindowTitle(L("Lorenz · Local file converter")); resize(1060, 820); setMinimumSize(870, 700);
        auto *root = new QWidget; setCentralWidget(root);
        auto *outer = new QHBoxLayout(root); outer->setContentsMargins(0,0,0,0); outer->setSpacing(0);
        auto *sidebar = new QFrame; sidebar->setObjectName("sidebar"); sidebar->setFixedWidth(190);
        auto *nav = new QVBoxLayout(sidebar); nav->setContentsMargins(22,30,20,24); nav->setSpacing(18);
        nav->addWidget(label("◉", "brand")); nav->addWidget(label("LORENZ", "brandName"));
        nav->addWidget(label(L("FILE CONVERTER"), "eyebrow")); nav->addSpacing(32);
        auto *active = label(L("▸  Convert"), "navActive"); nav->addWidget(active);
        auto *doctor = new QPushButton(L("Diagnostics")); nav->addWidget(doctor);
        auto *pin = new QCheckBox(L("Always on top")); pin->setToolTip(L("Subject to your desktop / compositor rules")); nav->addWidget(pin);
        nav->addStretch(); nav->addWidget(label(L("●  LOCAL PROCESSING"), "local"));
        nav->addWidget(label(L("Your files stay\non your computer."), "muted")); nav->addWidget(label("VERSION " LORENZ_VERSION, "eyebrow"));
        outer->addWidget(sidebar);
        auto *main = new QVBoxLayout; main->setContentsMargins(32,28,32,24); main->setSpacing(15); outer->addLayout(main,1);
        main->addWidget(label(L("YOUR DESKTOP. YOUR FILES."), "eyebrow"));
        main->addWidget(label(L("Change the format.\nKeep control."), "hero"));
        main->addWidget(label(L("Simple file conversion. No account. No uploads."), "muted"));
        auto *drop = new DropZone; main->addWidget(drop);
        auto *tools = new QHBoxLayout;
        browse = new QPushButton(L("＋  Add files")); tools->addWidget(browse);
        clear = new QPushButton(L("Clear")); tools->addWidget(clear); tools->addStretch();
        summary = label(L("No files"), "muted"); tools->addWidget(summary); main->addLayout(tools);
        table = new QTableWidget(0,3); table->setHorizontalHeaderLabels({L("FILE"), L("SIZE"), L("STATUS")});
        table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
        table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(2,QHeaderView::ResizeToContents);
        table->verticalHeader()->hide(); table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers); table->setShowGrid(false); table->setMinimumHeight(140); main->addWidget(table,1);
        auto *settings = new QHBoxLayout;
        auto *formatBox = new QVBoxLayout; formatBox->addWidget(label(L("OUTPUT FORMAT"), "eyebrow"));
        auto *formatRow = new QHBoxLayout; format = new QComboBox; format->setMinimumWidth(130); formatRow->addWidget(format,1);
        auto *wheel = new QPushButton("◉"); wheel->setToolTip(L("Open the format wheel")); formatRow->addWidget(wheel);
        formatBox->addLayout(formatRow); settings->addLayout(formatBox,1);
        auto *destBox = new QVBoxLayout; destBox->addWidget(label(L("SAVE TO"), "eyebrow"));
        choose = new QPushButton(L("Next to the originals  ▾")); destBox->addWidget(choose); settings->addLayout(destBox,2); main->addLayout(settings);
        destination = label(L("Originals are preserved. Only the extension changes; existing files are never replaced."), "muted"); main->addWidget(destination);
        progress = new QProgressBar; progress->setRange(0,1); progress->setValue(0); progress->setTextVisible(false); progress->setFixedHeight(4); main->addWidget(progress);
        auto *bottom = new QHBoxLayout; status = label(L("Ready to convert"), "muted"); bottom->addWidget(status,1);
        stop = new QPushButton(L("Cancel")); stop->setEnabled(false); bottom->addWidget(stop);
        start = new QPushButton(L("Convert  →")); start->setObjectName("primary"); start->setEnabled(false); bottom->addWidget(start); main->addLayout(bottom);
        auto *open = new QPushButton(L("Open last output folder")); main->addWidget(open,0,Qt::AlignRight);
        connect(open,&QPushButton::clicked,this,[this]{ if (!lastOutput.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(lastOutput).absolutePath())); });
        drop->receive = [this](const QStringList &files){ addFiles(files); };
        connect(browse,&QPushButton::clicked,this,[this]{ addFiles(QFileDialog::getOpenFileNames(this,L("Choose files"))); });
        connect(clear,&QPushButton::clicked,this,[this]{ table->setRowCount(0); refreshFormats(); });
        connect(choose,&QPushButton::clicked,this,[this]{
            QMenu menu; auto *original = menu.addAction(L("Next to the originals")); auto *custom = menu.addAction(L("Choose a folder…"));
            auto *selected = menu.exec(choose->mapToGlobal(QPoint(0,choose->height())));
            if (selected == original) outputDir.clear();
            else if (selected == custom) { auto dir = QFileDialog::getExistingDirectory(this,L("Output folder")); if (!dir.isEmpty()) outputDir = dir; }
            choose->setText(outputDir.isEmpty() ? L("Next to the originals  ▾") : QDir(outputDir).dirName() + "  ▾"); choose->setToolTip(outputDir);
        });
        connect(start,&QPushButton::clicked,this,[this]{
            if (format->count() == 0 || !cancellation.isValid()) return;
            QFile::remove(cancelPath()); current = 0; succeeded = failed = 0; cancelling = false; setBusy(true); next();
        });
        connect(stop,&QPushButton::clicked,this,[this]{ cancel(); });
        connect(&worker,&QProcess::readyReadStandardOutput,this,[this]{ response += worker.readAllStandardOutput(); });
        connect(&worker,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus){
            response += worker.readAllStandardOutput();
            const auto result = QJsonDocument::fromJson(response.trimmed()).object();
            const bool ok = code == 0 && result["ok"].toBool();
            auto *item = table->item(current,2);
            item->setText(ok ? L("Done") : cancelling ? L("Cancelled") : L("Failed"));
            item->setForeground(QColor(ok ? "#9bd7bc" : "#ff9299"));
            const QString error = result["error"].toString(QString::fromUtf8(worker.readAllStandardError()));
            item->setToolTip(ok ? result["output"].toString() : error);
            if (ok) { ++succeeded; lastOutput = result["output"].toString(); } else { ++failed; if (!cancelling) status->setText(error.left(180)); }
            ++current; next();
        });
        connect(&worker,&QProcess::errorOccurred,this,[this](QProcess::ProcessError e){
            if (e == QProcess::FailedToStart) { table->item(current,2)->setText(L("Core unavailable")); status->setText(worker.errorString()); setBusy(false); }
        });
        connect(doctor,&QPushButton::clicked,this,[this]{ showDoctor(); });
        connect(pin,&QCheckBox::toggled,this,[this](bool enabled){ setWindowFlag(Qt::WindowStaysOnTopHint, enabled); show(); });
        connect(wheel,&QPushButton::clicked,this,[this]{ showWheel(); });
        auto *shortcut = new QShortcut(QKeySequence("Ctrl+O"),this); connect(shortcut,&QShortcut::activated,browse,&QPushButton::click);
        auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape),this); connect(escape,&QShortcut::activated,this,[this]{ if (stop->isEnabled()) cancel(); });
        if (corePath().isEmpty()) status->setText(L("The Lorenz core is missing. Reinstall the application."));
    }
    QString cancelPath() const { return cancellation.path()+"/cancel"; }
    void testConversion() {
        const int index = format->findData("webp");
        if (index < 0) { QCoreApplication::exit(2); return; }
        format->setCurrentIndex(index); start->click();
        auto *timer = new QTimer(this);
        connect(timer,&QTimer::timeout,this,[this,timer]{
            if (current >= 0) return;
            timer->stop();
            const QString path=qEnvironmentVariable("LORENZ_SCREENSHOT");
            if (!path.isEmpty()) grab().save(path);
            QCoreApplication::exit(succeeded > 0 && failed == 0 ? 0 : 3);
        });
        timer->start(100);
        QTimer::singleShot(30000,this,[this]{ if (current >= 0) cancel(); });
    }
    void cancel() { cancelling = true; QFile f(cancelPath()); if (f.open(QIODevice::WriteOnly)) { f.close(); status->setText(L("Cancelling…")); stop->setEnabled(false); } }
    void setBusy(bool busy) {
        browse->setEnabled(!busy); clear->setEnabled(!busy); choose->setEnabled(!busy); format->setEnabled(!busy);
        stop->setEnabled(busy); start->setEnabled(!busy && format->count()>0); current = busy ? current : -1;
        progress->setRange(0,busy ? 0 : 1); progress->setValue(busy ? 0 : 1);
    }
    void next() {
        if (cancelling || current >= table->rowCount()) {
            if (cancelling) for (int i=current;i<table->rowCount();++i) table->item(i,2)->setText(L("Not processed"));
            status->setText(L("%1 completed · %2 failed%3").arg(succeeded).arg(failed).arg(cancelling ? L(" · queue stopped") : "")); setBusy(false); return;
        }
        response.clear(); table->item(current,2)->setText(L("Conversion…")); table->item(current,2)->setForeground(QColor("#edc16e"));
        status->setText(L("Processing %1 / %2").arg(current+1).arg(table->rowCount()));
        QStringList args {"convert", "--to", format->currentData().toString(), "--json", "--cancel-file", cancelPath()};
        if (!outputDir.isEmpty()) args << "--output-dir" << outputDir;
        args << "--" << table->item(current,0)->data(Qt::UserRole).toString(); worker.start(corePath(),args);
    }
    void addFiles(const QStringList &files) {
        if (current >= 0) return;
        for (const auto &path : files) {
            QFileInfo info(path); if (!info.isFile()) continue;
            const QString canonical = info.canonicalFilePath(); bool duplicate = false;
            for (int r=0;r<table->rowCount();++r) if (table->item(r,0)->data(Qt::UserRole).toString()==canonical) duplicate = true;
            if (duplicate) continue;
            const int row = table->rowCount(); table->insertRow(row);
            auto *file = new QTableWidgetItem(info.fileName()); file->setData(Qt::UserRole,canonical); file->setToolTip(canonical); table->setItem(row,0,file);
            table->setItem(row,1,new QTableWidgetItem(QLocale().formattedDataSize(info.size())));
            table->setItem(row,2,new QTableWidgetItem(L("Queued"))); table->setRowHeight(row,38);
        }
        refreshFormats();
    }
    void refreshFormats() {
        format->clear(); QSet<QString> common; QMap<QString,QString> names;
        for (int row=0;row<table->rowCount();++row) {
            const auto formats = query({"formats",table->item(row,0)->data(Qt::UserRole).toString(),"--json"}).array(); QSet<QString> targets;
            for (auto entry : formats) { const auto f=entry.toObject(); if (!f["available"].toBool()) continue;
                const QString target=f["target"].toString(); targets.insert(target); names[target]=target.toUpper()+" · "+f["category"].toString(); }
            if (row==0) common=targets; else common.intersect(targets);
        }
        QStringList sorted=common.values(); sorted.sort(); for (const auto &f : sorted) format->addItem(names[f],f);
        summary->setText(L("%1 file(s)").arg(table->rowCount())); start->setEnabled(!common.isEmpty());
        status->setText(table->rowCount()==0 ? L("Ready to convert") : common.isEmpty() ? L("No common format available. Check diagnostics.") : L("Choose a format, then start converting."));
    }
    void showDoctor() {
        const auto doc=query({"doctor","--json"}).object();
        QDialog dialog(this); dialog.setWindowTitle(L("Backend diagnostics")); dialog.resize(520,400); auto *l=new QVBoxLayout(&dialog);
        l->addWidget(label(L("Your local conversion engines"), "dropTitle"));
        if (doc.isEmpty()) l->addWidget(label(L("Cannot contact the Lorenz core.")));
        for (const auto &entry : doc["backends"].toArray()) { auto b=entry.toObject(); l->addWidget(label((b["available"].toBool()? "✓  " : "—  ")+b["name"].toString()+(b["available"].toBool()? L(" · available") : L(" · not installed")))); }
        l->addWidget(label(L("Codec support varies by installation. Missing engines disable their conversions. See the README for package details."),"muted"));
        auto *close=new QPushButton(L("Close")); l->addWidget(close); connect(close,&QPushButton::clicked,&dialog,&QDialog::accept); dialog.exec();
    }
    void showWheel() {
        if (format->count()==0 || current>=0) return;
        auto *wheel = new RadialWheel;
        wheel->setParent(this,wheel->windowFlags());
        QStringList values; for (int i=0;i<format->count();++i) values << format->itemData(i).toString();
        wheel->setFormats(values);
        wheel->sourceName=L("%1 files").arg(table->rowCount());
        wheel->chosen=[this,wheel](const QString &target) {
            if (!target.isEmpty()) format->setCurrentIndex(format->findData(target));
            QTimer::singleShot(120,wheel,&QObject::deleteLater);
        };
        wheel->openAt(QCursor::pos());
    }

    void closeEvent(QCloseEvent *event) override {
        if (current>=0) { cancel(); event->ignore(); status->setText(L("Cancelling. Close the window once processing has stopped.")); } else event->accept();
    }
};

// Wheel keys are shared with the KWin extension, which reads the same file.
static QString configPath() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)+"/lorenz/lorenz.conf";
}
static const QStringList wheelNames{"Convert","Tools","Actions"};
static const QStringList kinds{"convert","tools","actions"};
static QStringList wheelKeys() {
    QSettings settings(configPath(),QSettings::IniFormat);
    const QStringList defaults{"Shift","Control","Alt"}; QStringList keys;
    for (int i=0;i<3;++i) keys << settings.value("Wheels/"+wheelNames[i],defaults[i]).toString();
    return keys;
}
static QString keyLabel(const QString &key) { return key=="Control"?"Ctrl":key=="None"?"—":key; }
static QVector<RadialWheel::Item> menuItems(const QString &kind,const QStringList &files) {
    QVector<RadialWheel::Item> items;
    for (const auto entry:query(QStringList{"menu",kind,"--json","--"}+files).array()) {
        const auto item=entry.toObject();
        items.append({item["id"].toString(),item["label"].toString(),item["glyph"].toString()});
    }
    return items;
}

class SettingsDialog : public QDialog {
    QList<QComboBox*> keys;
    QCheckBox *login;
    QComboBox *language;
public:
    SettingsDialog() {
        setWindowTitle(L("Lorenz settings")); setFixedWidth(500);
        auto *layout=new QVBoxLayout(this); layout->setSpacing(12); layout->setContentsMargins(26,22,26,20);
        layout->addWidget(label(L("Wheels"),"dropTitle"));
        layout->addWidget(label(L("Start dragging a file, then tap a key. Each key opens its own wheel; tap another key to switch while it is open."),"muted"));
        auto *grid=new QFormLayout; const QStringList current=wheelKeys();
        const QStringList descriptions{L("Convert to another format"),L("Tools: compress, resize, rotate, split, merge…"),L("Actions: copy, move, share, terminal…")};
        for (int i=0;i<3;++i) {
            auto *box=new QComboBox; box->addItem(L("Shift"),"Shift"); box->addItem("Ctrl","Control"); box->addItem("Alt","Alt"); box->addItem(L("Off"),"None");
            box->setCurrentIndex(qMax(0,box->findData(current[i]))); box->setToolTip(descriptions[i]);
            keys << box; grid->addRow(L(wheelNames[i]),box);
        }
        layout->addLayout(grid);
        layout->addWidget(label(L("Startup"),"dropTitle"));
        login=new QCheckBox(L("Start Lorenz when I log in")); layout->addWidget(login);
        QProcess status; status.start(corePath(),{"autostart","status"}); status.waitForFinished(3000);
        login->setChecked(status.readAllStandardOutput().trimmed()=="enabled");
        layout->addWidget(label(L("The tray companion shows notifications and gives access to the batch manager. The KWin gesture works without it."),"muted"));
        layout->addWidget(label(L("Language"),"dropTitle"));
        language=new QComboBox; language->addItem(L("Automatic (system)"),"auto"); language->addItem("English","en"); language->addItem("Français","fr");
        language->setCurrentIndex(qMax(0,language->findData(lorenz::languageSetting()))); layout->addWidget(language);
        layout->addWidget(label(L("Applies the next time Lorenz starts."),"muted"));
        auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel); layout->addWidget(buttons);
        buttons->button(QDialogButtonBox::Save)->setText(L("Save")); buttons->button(QDialogButtonBox::Cancel)->setText(L("Cancel"));
        connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
        connect(buttons,&QDialogButtonBox::accepted,this,[this]{
            QStringList chosen; for (auto *box:keys) chosen << box->currentData().toString();
            for (int i=0;i<3;++i) for (int j=i+1;j<3;++j) if (chosen[i]!="None" && chosen[i]==chosen[j]) {
                QMessageBox::warning(this,"Lorenz",L("Each wheel needs a different key.")); return;
            }
            QSettings settings(configPath(),QSettings::IniFormat);
            for (int i=0;i<3;++i) settings.setValue("Wheels/"+wheelNames[i],chosen[i]);
            settings.setValue("Language",language->currentData().toString());
            settings.sync();
            QProcess::execute(corePath(),{"autostart",login->isChecked()?"enable":"disable"});
            accept();
        });
    }
};

int main(int argc,char **argv) {
    QApplication app(argc,argv); app.setApplicationName("Lorenz"); app.setOrganizationName("Lorenz");
    app.setStyle("Fusion"); app.setFont(QFont("DejaVu Sans",10));
    app.setStyleSheet(R"(
        QWidget { color:#edf3f1; background:#171d23; }
        QFrame#sidebar { background:#13191f; border-right:1px solid #2e3e44; }
        QLabel { background:transparent; }
        QLabel#brand { color:#b6e8d6; font-size:42px; font-weight:500; }
        QLabel#brandName { font-size:17px; font-weight:600; letter-spacing:2px; }
        QLabel#hero { font-size:30px; font-weight:600; }
        QLabel#eyebrow { color:#8fa2ab; font-size:10px; letter-spacing:2px; }
        QLabel#muted { color:#a9b9be; font-size:12px; }
        QLabel#local { color:#9bd7bc; font-size:10px; }
        QLabel#navActive { background:#293f3d; border-radius:9px; padding:12px; }
        QFrame#dropZone { background:#1d262d; border:1px dashed #496f68; border-radius:18px; }
        QLabel#dropSymbol { color:#b6e8d6; font-size:34px; }
        QLabel#dropTitle { font-size:17px; font-weight:600; }
        QPushButton { background:#273139; border:1px solid #3b4c54; padding:10px 12px; border-radius:9px; }
        QPushButton:hover { background:#33453f; border-color:#b6e8d6; }
        QPushButton:focus, QComboBox:focus { border:1px solid #b6e8d6; }
        QPushButton:disabled { color:#6b7c84; border-color:#2d3a42; }
        QPushButton#primary { color:#183c32; background:#b6e8d6; border-color:#b6e8d6; font-weight:700; padding:12px 25px; }
        QPushButton#primary:disabled { color:#6b7c84; background:#293f3d; border-color:#2e3e44; }
        QComboBox { background:#1d262d; border:1px solid #3b4c54; padding:10px; border-radius:9px; }
        QComboBox QAbstractItemView { background:#1d262d; selection-background-color:#496f68; }
        QComboBox::drop-down { border:0; width:30px; }
        QComboBox::down-arrow { image:url(:/chevron.png); width:12px; height:8px; }
        QCheckBox { spacing:10px; } QCheckBox::indicator { width:18px; height:18px; border-radius:5px; border:1px solid #496f68; background:#1d262d; }
        QCheckBox::indicator:checked { background:#b6e8d6; border-color:#b6e8d6; }
        QTableWidget { background:#1d262d; border:1px solid #2d3a42; selection-background-color:#293f3d; }
        QHeaderView::section { background:#273139; color:#8fa2ab; border:0; padding:10px; font-size:10px; }
        QProgressBar { border:0; background:#2d3a42; } QProgressBar::chunk { background:#b6e8d6; }
        QToolTip { background:#33453f; color:#edf3f1; border:1px solid #496f68; }
        QMenu { background:#1d262d; border:1px solid #3b4c54; } QMenu::item { padding:10px; } QMenu::item:selected { background:#496f68; }
    )");
    const auto args=app.arguments();
    QPixmap mark(64,64); mark.fill(Qt::transparent);
    { QPainter p(&mark); p.setRenderHint(QPainter::Antialiasing);
      p.setBrush(QColor("#1d262d")); p.setPen(Qt::NoPen); p.drawRoundedRect(QRectF(2,2,60,60),16,16);
      p.setPen(QPen(QColor("#b6e8d6"),6,Qt::SolidLine,Qt::RoundCap));
      for (int i=0;i<3;++i) p.drawArc(QRectF(14,14,36,36),(20+i*120)*16,88*16);
      p.setBrush(QColor("#edf3f1")); p.setPen(Qt::NoPen); p.drawEllipse(QPointF(32,32),5,5);
    }
    QSystemTrayIcon tray{QIcon(mark)};
    tray.setToolTip(L("Lorenz · drag a file, then tap Shift, Ctrl or Alt"));
    auto notify=[&](const QString &title,const QString &message) {
        const QString sender=QStandardPaths::findExecutable("notify-send");
        if (!sender.isEmpty()) QProcess::startDetached(sender,{"--app-name=Lorenz","--icon=lorenz",title,message});
        else if (QSystemTrayIcon::isSystemTrayAvailable()) tray.showMessage(title,message);
    };
    // A background job reports through notifications and can be cancelled from the tray.
    auto runJob=[&](QStringList command,const QStringList &paths,const QString &done,const QString &failed) {
        if (corePath().isEmpty()) return 2;
        app.setQuitOnLastWindowClosed(false);
        QProcess job; QTemporaryDir cancellationSignal; QByteArray result;
        if (!cancellationSignal.isValid()) return 2;
        QMenu menu; auto *cancel=menu.addAction(L("Cancel"));
        QObject::connect(cancel,&QAction::triggered,&app,[&]{ QFile flag(cancellationSignal.path()+"/cancel"); if (!flag.open(QIODevice::WriteOnly)) notify(L("Cancellation failed"),flag.errorString()); });
        tray.setContextMenu(&menu); tray.show();
        QObject::connect(&job,&QProcess::readyReadStandardOutput,&app,[&]{ result+=job.readAllStandardOutput(); });
        QObject::connect(&job,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),&app,[&](int code,QProcess::ExitStatus){
            result+=job.readAllStandardOutput(); QStringList outputs; QString failure;
            for (const auto &line:result.split('\n')) { if (line.trimmed().isEmpty()) continue;
                const auto item=QJsonDocument::fromJson(line).object();
                if (item["ok"].toBool()) outputs << QFileInfo(item["output"].toString()).fileName(); else failure=item["error"].toString();
            }
            const QString created=outputs.size()==1?outputs.first()+L(" created."):L("%1 items created.").arg(outputs.size());
            notify(code==0?done:failed,code==0?created+L(" Originals preserved."):failure.left(300));
            app.exit(code);
        });
        QObject::connect(&job,&QProcess::errorOccurred,&app,[&](QProcess::ProcessError e){ if (e==QProcess::FailedToStart) { notify(failed,job.errorString()); app.exit(2); } });
        command << "--cancel-file" << cancellationSignal.path()+"/cancel" << "--";
        job.start(corePath(),command+paths);
        return app.exec();
    };
    // Options taking a value; everything else that is not an option is a path.
    const QStringList valued{"--convert-to","--tool","--action","--wheel"};
    QStringList files;
    for (int i=1;i<args.size();++i) {
        if (valued.contains(args[i])) ++i;
        else if (args[i]=="--") { files+=args.mid(i+1); break; }
        else if (!args[i].startsWith("--")) files << args[i];
    }
    // Options are read only before "--": a file named like an option stays a file.
    const QStringList flags=args.mid(0,args.contains("--")?args.indexOf("--"):args.size());
    auto option=[&](const QString &name) { const int i=flags.indexOf(name); return i>=0 && i+1<flags.size()?flags[i+1]:QString(); };
    if (const QString target=option("--convert-to"); !target.isEmpty()) {
        return runJob({"convert","--json","--to",target},files,L("Conversion complete"),L("Conversion failed"));
    }
    if (const QString tool=option("--tool"); !tool.isEmpty()) {
        return runJob({"tool",tool,"--json"},files,L("Done"),L("The tool failed"));
    }
    if (const QString action=option("--action"); !action.isEmpty()) {
        if (files.isEmpty() || corePath().isEmpty()) return 2;
        QStringList command{"action",action,"--json"};
        if (action=="copy-to" || action=="move-to") {
            const QString folder=QFileDialog::getExistingDirectory(nullptr,action=="copy-to"?L("Copy to"):L("Move to"),QFileInfo(files.first()).absolutePath());
            if (folder.isEmpty()) return 0;
            command << "--dest" << folder;
        }
        QProcess job; job.start(corePath(),command+QStringList{"--"}+files);
        job.waitForFinished(-1);
        int count=0; QString failure;
        for (const auto &line:job.readAllStandardOutput().split('\n')) {
            const auto item=QJsonDocument::fromJson(line).object();
            if (item.contains("output")) ++count;
            if (item.contains("error")) failure=item["error"].toString();
        }
        if (job.exitCode()!=0) notify(L("Action failed"),failure.isEmpty()?QString::fromUtf8(job.readAllStandardError()).left(300):failure);
        else if (action=="copy") notify(L("Copied"),L("Ready to paste in your file manager."));
        else if (action=="path") notify(L("Path copied"),files.size()==1?files.first():L("%1 paths").arg(files.size()));
        else if (action=="copy-to") notify(L("Copied"),L("%1 item(s) copied. Nothing was replaced.").arg(count));
        else if (action=="move-to") notify(L("Moved"),L("%1 item(s) moved. Nothing was replaced.").arg(count));
        return job.exitCode();
    }
    if (flags.contains("--settings")) {
        SettingsDialog dialog;
        if (const QString path=qEnvironmentVariable("LORENZ_SCREENSHOT"); !path.isEmpty())
            QTimer::singleShot(400,&dialog,[&dialog,path]{ dialog.grab().save(path); dialog.reject(); });
        return dialog.exec()==QDialog::Accepted?0:1;
    }
    if (flags.contains("--manager") || flags.contains("--smoke-test") || flags.contains("--test-convert")) {
        Window window; window.addFiles(files); window.show();
        if (flags.contains("--test-convert")) QTimer::singleShot(100,&window,[&]{ window.testConversion(); });
        if (flags.contains("--smoke-test")) QTimer::singleShot(400,&app,[&]{ const QString path=qEnvironmentVariable("LORENZ_SCREENSHOT"); if (!path.isEmpty()) window.grab().save(path); app.quit(); });
        return app.exec();
    }
    if (!files.isEmpty()) {
        // File-manager entry point: the same three wheels, switched by key or tab.
        for (auto &file:files) file=QFileInfo(file).absoluteFilePath();
        RadialWheel wheel; QStringList keys=wheelKeys(); for (auto &k:keys) k=keyLabel(k); wheel.tabKeys=keys;
        int start=qMax(0,kinds.indexOf(option("--wheel")));
        wheel.sourceName=files.size()==1?QFileInfo(files.first()).fileName():L("%1 items").arg(files.size());
        wheel.sourceType=QFileInfo(files.first()).isDir()?"folder":QFileInfo(files.first()).suffix().toLower();
        wheel.message=L("Nothing available");
        auto load=[&](RadialWheel::Mode mode){ wheel.setMode(mode); wheel.setItems(menuItems(kinds[mode],files)); };
        load(RadialWheel::Mode(start));
        wheel.modeRequested=load;
        wheel.chosen=[&](const QString &id){
            if (!id.isEmpty()) {
                const QStringList flags{"--convert-to","--tool","--action"};
                QProcess::startDetached(app.applicationFilePath(),QStringList{flags[wheel.mode],id}+files);
            }
            QTimer::singleShot(120,&app,&QApplication::quit);
        };
        wheel.setWindowFlag(Qt::WindowDoesNotAcceptFocus,false);
        wheel.openAt(QCursor::pos()); wheel.activateWindow(); wheel.setFocus();
        return app.exec();
    }
    app.setQuitOnLastWindowClosed(false);
    QMenu menu;
    auto *settings=menu.addAction(L("Settings…"));
    QObject::connect(settings,&QAction::triggered,&app,[&]{ QProcess::startDetached(app.applicationFilePath(),{"--settings"}); });
    auto *manager=menu.addAction(L("Open batch manager"));
    QObject::connect(manager,&QAction::triggered,&app,[&]{ QProcess::startDetached(app.applicationFilePath(),{"--manager"}); });
    menu.addSeparator(); auto *quit=menu.addAction(L("Quit tray companion")); QObject::connect(quit,&QAction::triggered,&app,&QApplication::quit);
    tray.setContextMenu(&menu); tray.show();
#ifdef Q_OS_WIN
    // The companion hosts the drag gesture on Windows, as KWin does on Plasma.
    WindowsGesture gesture(corePath(),wheelKeys,keyLabel,[&](RadialWheel::Mode mode,const QString &id,const QStringList &paths){
        const QStringList options{"--convert-to","--tool","--action"};
        QProcess::startDetached(app.applicationFilePath(),QStringList{options[mode],id,"--"}+paths);
    });
#endif
    if (!flags.contains("--background")) {
        const QStringList keys=wheelKeys();
        notify("Lorenz",L("Drag a file, then tap %1 to convert, %2 for tools or %3 for actions.").arg(L(keyLabel(keys[0])),L(keyLabel(keys[1])),L(keyLabel(keys[2]))));
    }
    return app.exec();
}
