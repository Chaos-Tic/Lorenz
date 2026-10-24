#pragma once
#include <QtWidgets>
#include "i18n.h"

// KDE Plasma Wayland helper: checks whether the KWin drag-gesture plugin is
// loaded in the running compositor, automatically loads a matching installed
// plugin when KWin was updated, and locates the rebuild script when needed.
namespace kwin {

inline bool isKdeWayland() {
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
    if (qEnvironmentVariableIsSet("LORENZ_ISOLATED_TEST")) return false;
    return qEnvironmentVariable("XDG_SESSION_TYPE") == "wayland" &&
           qEnvironmentVariable("XDG_CURRENT_DESKTOP").contains("KDE", Qt::CaseInsensitive);
#else
    return false;
#endif
}

inline QString qdbusExecutable() {
    for (const char *name : {"qdbus6", "qdbus-qt6", "qdbus"}) {
        const QString found = QStandardPaths::findExecutable(name);
        if (!found.isEmpty()) return found;
    }
    for (const char *path : {"/usr/lib/qt6/bin/qdbus", "/usr/lib64/qt6/bin/qdbus"}) {
        if (QFileInfo(path).isExecutable()) return path;
    }
    return {};
}

inline QString pluginDirectory() {
    return QLibraryInfo::path(QLibraryInfo::PluginsPath) + "/kwin/plugins";
}

inline QString activeBuildId() {
    if (!isKdeWayland()) return {};
    const QString qdbus = qdbusExecutable();
    if (qdbus.isEmpty()) return {};
    QProcess p;
    p.start(qdbus, {"org.kde.KWin", "/Lorenz", "org.lorenz.KWin.BuildId"});
    if (!p.waitForFinished(1500) || p.exitCode() != 0) return {};
    const QString id = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
    return id.startsWith("lorenz-") ? id : QString();
}

inline void saveEnabledPlugin(const QString &activeId) {
    QSettings kwinrc(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/kwinrc",
                     QSettings::IniFormat);
    kwinrc.beginGroup("Plugins");
    for (const QString &key : kwinrc.childKeys()) {
        if (key == "lorenzEnabled" || (key.startsWith("lorenz-") && key.endsWith("Enabled"))) {
            kwinrc.remove(key);
        }
    }
    kwinrc.setValue(activeId + "Enabled", true);
    kwinrc.endGroup();
    kwinrc.sync();
}

// If the KWin extension is not yet active in the compositor, try loading any
// installed lorenz-<revision>.so that matches the running KWin version (such as
// one built by the package manager's post-transaction hook).
inline QString ensureLoaded() {
    if (!isKdeWayland()) return {};
    const QString current = activeBuildId();
    if (!current.isEmpty()) return current;
    const QString qdbus = qdbusExecutable();
    if (qdbus.isEmpty()) return {};
    QDir dir(pluginDirectory());
    const QFileInfoList candidates = dir.entryInfoList({"lorenz-*.so"}, QDir::Files, QDir::Time);
    for (const QFileInfo &info : candidates) {
        const QString id = info.completeBaseName();
        QProcess load;
        load.start(qdbus, {"org.kde.KWin", "/Plugins", "org.kde.KWin.Plugins.LoadPlugin", id});
        if (!load.waitForFinished(2000) || load.exitCode() != 0) continue;
        if (QString::fromUtf8(load.readAllStandardOutput()).trimmed() == "true" && activeBuildId() == id) {
            saveEnabledPlugin(id);
            return id;
        }
    }
    return {};
}

inline QString installScriptPath() {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates{
        appDir + "/../scripts/install-kwin.sh",
        appDir + "/../share/lorenz/scripts/install-kwin.sh",
        "/usr/share/lorenz/scripts/install-kwin.sh",
        QDir::homePath() + "/.local/share/lorenz/scripts/install-kwin.sh",
    };
    for (const QString &path : candidates) {
        const QFileInfo info(path);
        if (info.isFile() && info.isExecutable()) return info.canonicalFilePath();
    }
    return {};
}

} // namespace kwin
