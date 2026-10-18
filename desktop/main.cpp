#include <QtWidgets>
#include <cmath>

static QString corePath() {
    QString name = "metroid";
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
        auto *title = label("Déposez vos fichiers ici", "dropTitle"); title->setAlignment(Qt::AlignCenter);
        auto *hint = label("Images, audio, vidéo, documents et PDF", "muted"); hint->setAlignment(Qt::AlignCenter);
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
        setWindowTitle("Metroid · Convertisseur local"); resize(1060, 820); setMinimumSize(870, 700);
        auto *root = new QWidget; setCentralWidget(root);
        auto *outer = new QHBoxLayout(root); outer->setContentsMargins(0,0,0,0); outer->setSpacing(0);
        auto *sidebar = new QFrame; sidebar->setObjectName("sidebar"); sidebar->setFixedWidth(190);
        auto *nav = new QVBoxLayout(sidebar); nav->setContentsMargins(22,30,20,24); nav->setSpacing(18);
        nav->addWidget(label("M /", "brand")); nav->addWidget(label("METROID", "brandName"));
        nav->addWidget(label("FILE CONVERTER", "eyebrow")); nav->addSpacing(32);
        auto *active = label("▸  Convertir", "navActive"); nav->addWidget(active);
        auto *doctor = new QPushButton("Diagnostic"); nav->addWidget(doctor);
        auto *pin = new QCheckBox("Toujours visible"); pin->setToolTip("Selon les règles de votre bureau / compositeur"); nav->addWidget(pin);
        nav->addStretch(); nav->addWidget(label("●  TRAITEMENT LOCAL", "local"));
        nav->addWidget(label("Vos fichiers restent\nsur votre ordinateur.", "muted")); nav->addWidget(label("VERSION 0.1.0", "eyebrow"));
        outer->addWidget(sidebar);
        auto *main = new QVBoxLayout; main->setContentsMargins(32,28,32,24); main->setSpacing(15); outer->addLayout(main,1);
        main->addWidget(label("VOTRE BUREAU. VOS FICHIERS.", "eyebrow"));
        main->addWidget(label("Changez de format.\nGardez le contrôle.", "hero"));
        main->addWidget(label("Une conversion simple, sans compte et sans téléversement.", "muted"));
        auto *drop = new DropZone; main->addWidget(drop);
        auto *tools = new QHBoxLayout;
        browse = new QPushButton("＋  Ajouter des fichiers"); tools->addWidget(browse);
        clear = new QPushButton("Vider"); tools->addWidget(clear); tools->addStretch();
        summary = label("Aucun fichier", "muted"); tools->addWidget(summary); main->addLayout(tools);
        table = new QTableWidget(0,3); table->setHorizontalHeaderLabels({"FICHIER", "TAILLE", "ÉTAT"});
        table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
        table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(2,QHeaderView::ResizeToContents);
        table->verticalHeader()->hide(); table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers); table->setShowGrid(false); table->setMinimumHeight(140); main->addWidget(table,1);
        auto *settings = new QHBoxLayout;
        auto *formatBox = new QVBoxLayout; formatBox->addWidget(label("FORMAT DE SORTIE", "eyebrow"));
        auto *formatRow = new QHBoxLayout; format = new QComboBox; format->setMinimumWidth(130); formatRow->addWidget(format,1);
        auto *wheel = new QPushButton("◉"); wheel->setToolTip("Ouvrir la roue des formats"); formatRow->addWidget(wheel);
        formatBox->addLayout(formatRow); settings->addLayout(formatBox,1);
        auto *destBox = new QVBoxLayout; destBox->addWidget(label("ENREGISTRER DANS", "eyebrow"));
        choose = new QPushButton("À côté des originaux  ▾"); destBox->addWidget(choose); settings->addLayout(destBox,2); main->addLayout(settings);
        destination = label("Les originaux sont conservés. Les résultats portent le suffixe -metroid.", "muted"); main->addWidget(destination);
        progress = new QProgressBar; progress->setRange(0,1); progress->setValue(0); progress->setTextVisible(false); progress->setFixedHeight(4); main->addWidget(progress);
        auto *bottom = new QHBoxLayout; status = label("Prêt à convertir", "muted"); bottom->addWidget(status,1);
        stop = new QPushButton("Annuler"); stop->setEnabled(false); bottom->addWidget(stop);
        start = new QPushButton("Convertir  →"); start->setObjectName("primary"); start->setEnabled(false); bottom->addWidget(start); main->addLayout(bottom);
        auto *open = new QPushButton("Ouvrir le dernier résultat"); main->addWidget(open,0,Qt::AlignRight);
        connect(open,&QPushButton::clicked,this,[this]{ if (!lastOutput.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(lastOutput).absolutePath())); });
        drop->receive = [this](const QStringList &files){ addFiles(files); };
        connect(browse,&QPushButton::clicked,this,[this]{ addFiles(QFileDialog::getOpenFileNames(this,"Choisir des fichiers")); });
        connect(clear,&QPushButton::clicked,this,[this]{ table->setRowCount(0); refreshFormats(); });
        connect(choose,&QPushButton::clicked,this,[this]{
            QMenu menu; auto *original = menu.addAction("À côté des originaux"); auto *custom = menu.addAction("Choisir un dossier…");
            auto *selected = menu.exec(choose->mapToGlobal(QPoint(0,choose->height())));
            if (selected == original) outputDir.clear();
            else if (selected == custom) { auto dir = QFileDialog::getExistingDirectory(this,"Dossier de sortie"); if (!dir.isEmpty()) outputDir = dir; }
            choose->setText(outputDir.isEmpty() ? "À côté des originaux  ▾" : QDir(outputDir).dirName() + "  ▾"); choose->setToolTip(outputDir);
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
            item->setText(ok ? "Terminé" : cancelling ? "Annulé" : "Échec");
            item->setForeground(QColor(ok ? "#5fdda0" : "#ff9299"));
            const QString error = result["error"].toString(QString::fromUtf8(worker.readAllStandardError()));
            item->setToolTip(ok ? result["output"].toString() : error);
            if (ok) { ++succeeded; lastOutput = result["output"].toString(); } else { ++failed; if (!cancelling) status->setText(error.left(180)); }
            ++current; next();
        });
        connect(&worker,&QProcess::errorOccurred,this,[this](QProcess::ProcessError e){
            if (e == QProcess::FailedToStart) { table->item(current,2)->setText("Moteur introuvable"); status->setText(worker.errorString()); setBusy(false); }
        });
        connect(doctor,&QPushButton::clicked,this,[this]{ showDoctor(); });
        connect(pin,&QCheckBox::toggled,this,[this](bool enabled){ setWindowFlag(Qt::WindowStaysOnTopHint, enabled); show(); });
        connect(wheel,&QPushButton::clicked,this,[this]{ showWheel(); });
        auto *shortcut = new QShortcut(QKeySequence("Ctrl+O"),this); connect(shortcut,&QShortcut::activated,browse,&QPushButton::click);
        auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape),this); connect(escape,&QShortcut::activated,this,[this]{ if (stop->isEnabled()) cancel(); });
        if (corePath().isEmpty()) status->setText("Le cœur metroid est absent. Réinstallez l'application.");
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
            const QString path=qEnvironmentVariable("METROID_SCREENSHOT");
            if (!path.isEmpty()) grab().save(path);
            QCoreApplication::exit(succeeded > 0 && failed == 0 ? 0 : 3);
        });
        timer->start(100);
        QTimer::singleShot(30000,this,[this]{ if (current >= 0) cancel(); });
    }
    void cancel() { cancelling = true; QFile f(cancelPath()); if (f.open(QIODevice::WriteOnly)) { f.close(); status->setText("Annulation en cours…"); stop->setEnabled(false); } }
    void setBusy(bool busy) {
        browse->setEnabled(!busy); clear->setEnabled(!busy); choose->setEnabled(!busy); format->setEnabled(!busy);
        stop->setEnabled(busy); start->setEnabled(!busy && format->count()>0); current = busy ? current : -1;
        progress->setRange(0,busy ? 0 : 1); progress->setValue(busy ? 0 : 1);
    }
    void next() {
        if (cancelling || current >= table->rowCount()) {
            if (cancelling) for (int i=current;i<table->rowCount();++i) table->item(i,2)->setText("Non traité");
            status->setText(QString("%1 réussi(s) · %2 échec(s)%3").arg(succeeded).arg(failed).arg(cancelling ? " · file interrompue" : "")); setBusy(false); return;
        }
        response.clear(); table->item(current,2)->setText("Conversion…"); table->item(current,2)->setForeground(QColor("#edc16e"));
        status->setText(QString("Traitement %1 / %2").arg(current+1).arg(table->rowCount()));
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
            table->setItem(row,2,new QTableWidgetItem("En attente")); table->setRowHeight(row,38);
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
        summary->setText(QString("%1 fichier(s)").arg(table->rowCount())); start->setEnabled(!common.isEmpty());
        status->setText(table->rowCount()==0 ? "Prêt à convertir" : common.isEmpty() ? "Aucun format commun disponible. Consultez le diagnostic." : "Choisissez un format, puis lancez la conversion.");
    }
    void showDoctor() {
        const auto doc=query({"doctor","--json"}).object();
        QDialog dialog(this); dialog.setWindowTitle("Diagnostic des moteurs"); dialog.resize(520,400); auto *l=new QVBoxLayout(&dialog);
        l->addWidget(label("Les moteurs de votre ordinateur", "dropTitle"));
        if (doc.isEmpty()) l->addWidget(label("Impossible de contacter le cœur Metroid."));
        for (const auto &entry : doc["backends"].toArray()) { auto b=entry.toObject(); l->addWidget(label((b["available"].toBool()? "✓  " : "—  ")+b["name"].toString()+(b["available"].toBool()? " · disponible" : " · non installé"))); }
        l->addWidget(label("Les codecs varient selon l'installation. Un moteur absent désactive les conversions associées. Consultez le README pour les paquets.","muted"));
        auto *close=new QPushButton("Fermer"); l->addWidget(close); connect(close,&QPushButton::clicked,&dialog,&QDialog::accept); dialog.exec();
    }
    void showWheel() {
        if (format->count()==0 || current>=0) return;
        QDialog dialog(this); dialog.setWindowTitle("Choisir un format"); dialog.setFixedSize(440,440);
        auto *center=label("METROID\nFORMAT", "brandName"); center->setParent(&dialog); center->setAlignment(Qt::AlignCenter); center->setGeometry(150,185,140,60);
        for (int i=0;i<format->count();++i) {
            const double angle=-1.57079632679+i*6.28318530718/format->count();
            auto *button=new QPushButton(format->itemData(i).toString().toUpper(),&dialog);
            button->setGeometry(int(220+155*std::cos(angle))-43,int(220+155*std::sin(angle))-22,86,44);
            connect(button,&QPushButton::clicked,&dialog,[&,i]{ format->setCurrentIndex(i); dialog.accept(); });
        }
        dialog.exec();
    }
    void closeEvent(QCloseEvent *event) override {
        if (current>=0) { cancel(); event->ignore(); status->setText("Annulation en cours. Fermez après l'arrêt du traitement."); } else event->accept();
    }
};

int main(int argc,char **argv) {
    QApplication app(argc,argv); app.setApplicationName("Metroid"); app.setOrganizationName("Metroid");
    app.setStyle("Fusion"); app.setFont(QFont("DejaVu Sans",10));
    app.setStyleSheet(R"(
        QWidget { color:#f2eeee; background:#080808; }
        QFrame#sidebar { background:#030303; border-right:1px solid #413030; }
        QLabel { background:transparent; }
        QLabel#brand { color:#ff5454; font-size:48px; font-weight:800; }
        QLabel#brandName { font-size:18px; font-weight:800; letter-spacing:3px; }
        QLabel#hero { font-size:31px; font-weight:750; }
        QLabel#eyebrow { color:#a39797; font-size:10px; letter-spacing:2px; }
        QLabel#muted { color:#bdb3b3; font-size:12px; }
        QLabel#local { color:#5fdda0; font-size:10px; }
        QLabel#navActive { background:#2a1013; border-left:3px solid #ff5454; padding:12px; }
        QFrame#dropZone { background:#101010; border:1px dashed #773034; }
        QLabel#dropSymbol { color:#ff5454; font-size:34px; }
        QLabel#dropTitle { font-size:17px; font-weight:600; }
        QPushButton { background:#161414; border:1px solid #413b3b; padding:10px 12px; }
        QPushButton:hover { background:#231719; border-color:#ff5454; }
        QPushButton:focus, QComboBox:focus { border:1px solid #ff5454; }
        QPushButton:disabled { color:#766969; border-color:#242020; }
        QPushButton#primary { background:#b91624; border-color:#b91624; font-weight:700; padding:12px 25px; }
        QPushButton#primary:disabled { background:#2a1013; border-color:#413030; }
        QComboBox { background:#101010; border:1px solid #413b3b; padding:10px; }
        QComboBox QAbstractItemView { background:#101010; selection-background-color:#773034; }
        QTableWidget { background:#101010; border:1px solid #242020; selection-background-color:#2a1013; }
        QHeaderView::section { background:#161414; color:#a39797; border:0; padding:10px; font-size:10px; }
        QProgressBar { border:0; background:#242020; } QProgressBar::chunk { background:#ff5454; }
        QToolTip { background:#231719; color:#f2eeee; border:1px solid #773034; }
        QMenu { background:#101010; border:1px solid #413b3b; } QMenu::item { padding:10px; } QMenu::item:selected { background:#773034; }
    )");
    Window window; QStringList files;
    const auto args=app.arguments(); bool smoke=args.contains("--smoke-test");
    for (int i=1;i<args.size();++i) if (!args[i].startsWith("--")) files << args[i];
    window.addFiles(files); window.show();
    if (args.contains("--test-convert")) QTimer::singleShot(100,&window,[&]{ window.testConversion(); });
    if (smoke) QTimer::singleShot(400,&app,[&]{ const QString path=qEnvironmentVariable("METROID_SCREENSHOT"); if (!path.isEmpty()) window.grab().save(path); app.quit(); });
    return app.exec();
}
