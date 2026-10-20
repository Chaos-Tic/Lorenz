#pragma once
#include <QtNetwork>
#include <QtWidgets>
#include <functional>
#include "i18n.h"

// Update check: at most once a day, one anonymous HTTPS request for the latest
// release of the GitHub repository. Nothing about the user or their files is
// sent; GitHub sees the request's address, as for any download. It can be
// turned off ("CheckUpdates" in lorenz.conf, or Settings). Conversions and
// the core (`lorenz`) never use the network; only the desktop does this.
namespace update {

inline const char *releasesApi = "https://api.github.com/repos/Chaos-Tic/Lorenz/releases/latest";
inline const char *releasesPage = "https://github.com/Chaos-Tic/Lorenz/releases/latest";

inline QSettings &settings() {
    static QSettings s(lorenz::configFile(), QSettings::IniFormat);
    return s;
}
inline bool enabled() { return settings().value("CheckUpdates", true).toBool(); }
inline void setEnabled(bool on) { settings().setValue("CheckUpdates", on); settings().sync(); }

// "v1.2.3", "1.2", "1.2.3-rc1" -> (1, 2, 3); missing parts are 0.
inline QList<int> parts(QString v) {
    v = v.trimmed();
    if (v.startsWith('v') || v.startsWith('V')) v.remove(0, 1);
    v = v.section('-', 0, 0).section('+', 0, 0);
    QList<int> out;
    for (const QString &p : v.split('.')) {
        bool ok = false; const int n = p.toInt(&ok);
        out << (ok ? n : 0);
    }
    while (out.size() < 3) out << 0;
    return out.mid(0, 3);
}

inline bool newer(const QString &candidate, const QString &current) {
    const QList<int> a = parts(candidate), b = parts(current);
    for (int i = 0; i < 3; ++i) if (a[i] != b[i]) return a[i] > b[i];
    return false;
}

// How this copy was installed, so the user is shown the right way to update.
struct Method { QString how, command; bool download = false; };

inline Method method(const QString &exeDir = QCoreApplication::applicationDirPath()) {
    if (qEnvironmentVariableIsSet("APPIMAGE"))
        return {L("Download the new AppImage and replace this one."), {}, true};
#ifdef Q_OS_WIN
    Q_UNUSED(exeDir);
    return {L("Update with winget, or download the new installer."), "winget upgrade ChaosTic.Lorenz", true};
#else
    if (exeDir.startsWith(QDir::homePath()))
        return {L("Lorenz was built from source: update the checkout and install it again."),
            "git pull && ./install.sh", false};
    auto has = [](const char *tool) { return !QStandardPaths::findExecutable(tool).isEmpty(); };
    if (has("pacman"))
        return {L("Update with your package manager."), has("yay") ? "yay -Syu lorenz" : "paru -Syu lorenz", false};
    if (has("dnf")) return {L("Update with your package manager."), "sudo dnf upgrade lorenz", false};
    if (has("apt-get")) return {L("Update with your package manager."), "sudo apt update && sudo apt install --only-upgrade lorenz", false};
    if (has("zypper")) return {L("Update with your package manager."), "sudo zypper update lorenz", false};
    return {L("Download the new version from GitHub."), {}, true};
#endif
}

// Asks GitHub for the latest release when the last check is older than a day
// (or `force`), then calls `found` with its version when it is newer than
// `current`. LORENZ_UPDATE_URL replaces the address (tests).
class Checker : public QObject {
public:
    std::function<void(QString version, QString page)> found;
    explicit Checker(QObject *parent = nullptr) : QObject(parent) {}

    void check(const QString &current, bool force = false) {
        if (!enabled() || qEnvironmentVariableIsSet("LORENZ_ISOLATED_TEST")) return;
        QSettings &s = settings();
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        const qint64 last = s.value("LastUpdateCheck", 0).toLongLong();
        const QString cached = s.value("LatestVersion").toString();
        if (!force && now - last < 24 * 3600) {
            if (!cached.isEmpty() && newer(cached, current) && found) found(cached, s.value("LatestPage", releasesPage).toString());
            return;
        }
        const QString address = qEnvironmentVariable("LORENZ_UPDATE_URL", releasesApi);
        QNetworkRequest request{QUrl(address)};
        request.setHeader(QNetworkRequest::UserAgentHeader, "Lorenz");
        request.setRawHeader("Accept", "application/vnd.github+json");
        request.setTransferTimeout(10000);
        QNetworkReply *reply = net.get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply, current, now] {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) return;
            const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
            const QString tag = o["tag_name"].toString();
            if (tag.isEmpty() || o["draft"].toBool() || o["prerelease"].toBool()) return;
            const QString version = tag.startsWith('v') ? tag.mid(1) : tag;
            const QString page = o["html_url"].toString(releasesPage);
            QSettings &s = settings();
            s.setValue("LastUpdateCheck", now); s.setValue("LatestVersion", version); s.setValue("LatestPage", page); s.sync();
            if (newer(version, current) && found) found(version, page);
        });
    }

private:
    QNetworkAccessManager net;
};

} // namespace update
