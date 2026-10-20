// The update check without the network: versions, a release served from a
// local file, the daily cache, and the off switch.
#include <QtWidgets>
#include "../desktop/update.h"

static int failures = 0;
static void check(bool ok, const char *what) {
    if (!ok) { ++failures; qWarning("FAIL: %s", what); } else qInfo("ok  %s", what);
}

static QString waitFor(const QString &current, bool force) {
    QString seen;
    update::Checker c;
    c.found = [&](const QString &v, const QString &) { seen = v; };
    c.check(current, force);
    QElapsedTimer t; t.start();
    while (seen.isEmpty() && t.elapsed() < 1500) QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    return seen;
}

int main(int argc, char **argv) {
    QTemporaryDir config; qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
    QApplication app(argc, argv);
    check(update::newer("v0.5.3", "0.1.0"), "0.5.3 is newer than 0.1.0");
    check(update::newer("1.0.0", "0.9.9"), "1.0.0 is newer than 0.9.9");
    check(update::newer("0.10.0", "0.9.0"), "versions compare by number, not text");
    check(!update::newer("0.1.0", "0.1.0"), "the same version is not newer");
    check(!update::newer("v0.1.0-rc1", "0.1.0"), "a pre-release suffix does not make it newer");

    QTemporaryDir dir;
    QFile f(dir.filePath("latest.json")); f.open(QIODevice::WriteOnly);
    f.write(R"({"tag_name":"v0.5.3","html_url":"https://example.invalid/r","draft":false,"prerelease":false})"); f.close();
    qputenv("LORENZ_UPDATE_URL", QUrl::fromLocalFile(f.fileName()).toString().toUtf8());

    check(waitFor("0.1.0", true) == "0.5.3", "a newer release is offered");
    check(waitFor("0.5.3", true).isEmpty(), "nothing is offered when up to date");
    QFile::remove(f.fileName());
    check(waitFor("0.1.0", false) == "0.5.3", "within a day the cached answer is used, without asking again");
    update::setEnabled(false);
    check(waitFor("0.1.0", true).isEmpty(), "nothing is checked when updates are turned off");
    return failures ? 1 : 0;
}
