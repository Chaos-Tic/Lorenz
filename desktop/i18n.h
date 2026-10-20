#pragma once
#include <QtCore>

// Interface language. "Language" in lorenz.conf is "fr", "en" or "auto";
// the Windows installer writes the language picked during setup, and
// "auto" (the default) follows the system. Text is written in English in
// the code; L() returns the French text when the interface is French.
// The KWin extension shares this header and the configuration file.
namespace lorenz {
inline QString configFile() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/lorenz/lorenz.conf";
}
inline QString languageSetting() {
    return QSettings(configFile(), QSettings::IniFormat).value("Language", "auto").toString();
}
inline bool french() {
    static const bool value = [] {
        const QString chosen = languageSetting();
        if (chosen == "fr" || chosen == "en") return chosen == "fr";
        return QLocale::system().language() == QLocale::French;
    }();
    return value;
}
inline QString L(const QString &english) {
    if (!french()) return english;
    static const QHash<QString, QString> fr{
        // Updates
        {"Update Lorenz", "Mettre à jour Lorenz"}, {"Lorenz %1 is available. You have %2.", "Lorenz %1 est disponible. Vous avez la %2."},
        {"Lorenz %1 is available", "Lorenz %1 est disponible"}, {"Update to %1", "Mettre à jour vers %1"},
        {"Update to %1…", "Mettre à jour vers %1…"}, {"Copy the command", "Copier la commande"},
        {"Open the release page", "Ouvrir la page de la version"},
        {"Check for updates once a day", "Vérifier les mises à jour une fois par jour"},
        {"Update with your package manager.", "Mettez à jour avec votre gestionnaire de paquets."},
        {"Download the new AppImage and replace this one.", "Téléchargez la nouvelle AppImage et remplacez celle-ci."},
        {"Update with winget, or download the new installer.", "Mettez à jour avec winget, ou téléchargez le nouvel installateur."},
        {"Lorenz was built from source: update the checkout and install it again.",
         "Lorenz a été compilé depuis les sources : mettez à jour le dépôt et réinstallez-le."},
        {"Download the new version from GitHub.", "Téléchargez la nouvelle version sur GitHub."},
        // Wheel
        {"Convert", "Convertir"}, {"Tools", "Outils"}, {"Actions", "Actions"},
        {"Release to convert", "Relâcher pour convertir"}, {"Release to apply", "Relâcher pour appliquer"},
        {"Release to run", "Relâcher pour lancer"}, {"Reading…", "Lecture…"}, {"Finding options", "Recherche des options"},
        {"Unavailable", "Indisponible"}, {"Cancel", "Annuler"}, {"Shift", "Maj"},
        {"Reading files…", "Lecture des fichiers…"}, {"Nothing available", "Rien de disponible"},
        {"No common format", "Aucun format commun"}, {"Engine unavailable", "Moteur indisponible"},
        {"%1 items", "%1 éléments"},
        // Format families and hints
        {"Universal", "Universel"}, {"Opens anywhere", "S'ouvre partout"}, {"Web", "Web"},
        {"Smaller, keeps quality", "Plus léger, même qualité"}, {"Smallest", "Le plus léger"},
        {"Smallest file, newer apps", "Fichier minimal, applis récentes"}, {"Lossless", "Sans perte"},
        {"Sharp edges, transparency", "Contours nets, transparence"}, {"Document", "Document"},
        {"One page per image", "Une page par image"}, {"Lossless, for print", "Sans perte, pour l'impression"},
        {"Compact", "Compact"}, {"Compact, phone photos", "Compact, photos de téléphone"},
        {"Animation", "Animation"}, {"Short loops", "Boucles courtes"}, {"Bitmap", "Bitmap"}, {"Uncompressed", "Non compressé"},
        {"Plays everywhere", "Se lit partout"}, {"Small, good quality", "Léger, bonne qualité"},
        {"Exact copy of the sound", "Copie exacte du son"}, {"Raw", "Brut"}, {"Uncompressed audio", "Audio non compressé"},
        {"Open format", "Format ouvert"}, {"Smallest for speech", "Le plus léger pour la voix"},
        {"Open web video", "Vidéo web ouverte"}, {"Keeps every track", "Garde toutes les pistes"},
        {"Apple devices", "Appareils Apple"}, {"Editable document", "Document modifiable"},
        {"Plain text with formatting", "Texte avec mise en forme"}, {"Web page", "Page web"},
        {"Opens in a browser", "S'ouvre dans un navigateur"}, {"Text", "Texte"}, {"Plain text", "Texte brut"},
        {"E-book", "Livre numérique"}, {"For readers", "Pour liseuses"},
        // Tools and actions (labels from the core, and their hints)
        {"Compress", "Compresser"}, {"Same pixels, lighter file", "Mêmes pixels, fichier plus léger"},
        {"Resize 50%", "Redimensionner 50 %"}, {"Half size", "Moitié de la taille"},
        {"Half the width and height", "Moitié de la largeur et de la hauteur"},
        {"Rotate right", "Pivoter à droite"}, {"90° clockwise", "90° dans le sens horaire"},
        {"Rotate left", "Pivoter à gauche"}, {"90° anticlockwise", "90° dans le sens antihoraire"},
        {"Mirror", "Miroir"}, {"Flip left to right", "Retourner horizontalement"},
        {"Black & white", "Noir et blanc"}, {"Greyscale copy", "Copie en niveaux de gris"},
        {"Remove metadata", "Retirer les métadonnées"}, {"Removes GPS and camera data", "Retire GPS et infos de l'appareil"},
        {"Compress to ZIP", "Compresser en ZIP"}, {"One archive beside it", "Une archive à côté"},
        {"Remove audio", "Retirer le son"}, {"Speed ×2", "Vitesse ×2"}, {"First frame", "Première image"},
        {"Normalize volume", "Normaliser le volume"}, {"Mono", "Mono"}, {"Merge", "Fusionner"},
        {"Split pages", "Séparer les pages"}, {"Pages to PNG", "Pages en PNG"}, {"Extract", "Extraire"},
        {"Share", "Partager"}, {"Float", "Épingler"}, {"Copy", "Copier"}, {"To the clipboard", "Dans le presse-papiers"},
        {"Copy to…", "Copier vers…"}, {"Move to…", "Déplacer vers…"}, {"Pick a folder", "Choisir un dossier"},
        {"Copy path", "Copier le chemin"}, {"Full path as text", "Chemin complet en texte"},
        {"Terminal", "Terminal"}, {"Opens in this folder", "S'ouvre dans ce dossier"},
        {"Show in folder", "Afficher dans le dossier"}, {"Reveal in the file manager", "Dans le gestionnaire de fichiers"},
        // Batch window
        {"Lorenz · Local file converter", "Lorenz · Convertisseur de fichiers local"},
        {"Drop your files here", "Déposez vos fichiers ici"},
        {"Images, audio, video, documents and PDF", "Images, audio, vidéo, documents et PDF"},
        {"FILE CONVERTER", "CONVERTISSEUR DE FICHIERS"}, {"▸  Convert", "▸  Convertir"}, {"Diagnostics", "Diagnostic"},
        {"Always on top", "Toujours au premier plan"}, {"Subject to your desktop / compositor rules", "Selon les règles de votre bureau"},
        {"●  LOCAL PROCESSING", "●  TRAITEMENT LOCAL"}, {"Your files stay\non your computer.", "Vos fichiers restent\nsur votre ordinateur."},
        {"YOUR DESKTOP. YOUR FILES.", "VOTRE BUREAU. VOS FICHIERS."}, {"Change the format.\nKeep control.", "Changez de format.\nGardez la main."},
        {"Simple file conversion. No account. No uploads.", "Conversion simple. Sans compte. Sans envoi en ligne."},
        {"＋  Add files", "＋  Ajouter des fichiers"}, {"Clear", "Vider"}, {"No files", "Aucun fichier"},
        {"FILE", "FICHIER"}, {"SIZE", "TAILLE"}, {"STATUS", "ÉTAT"}, {"OUTPUT FORMAT", "FORMAT DE SORTIE"},
        {"Open the format wheel", "Ouvrir la roue des formats"}, {"SAVE TO", "ENREGISTRER DANS"},
        {"Next to the originals  ▾", "À côté des originaux  ▾"}, {"Next to the originals", "À côté des originaux"},
        {"Originals are preserved. Only the extension changes; existing files are never replaced.",
         "Les originaux sont conservés. Seule l'extension change ; aucun fichier existant n'est remplacé."},
        {"Ready to convert", "Prêt à convertir"}, {"Convert  →", "Convertir  →"},
        {"Open last output folder", "Ouvrir le dernier dossier de sortie"}, {"Choose files", "Choisir des fichiers"},
        {"Choose a folder…", "Choisir un dossier…"}, {"Output folder", "Dossier de sortie"},
        {"Done", "Terminé"}, {"Cancelled", "Annulé"}, {"Failed", "Échec"}, {"Core unavailable", "Moteur Lorenz indisponible"},
        {"The Lorenz core is missing. Reinstall the application.", "Le moteur Lorenz est absent. Réinstallez l'application."},
        {"Cancelling…", "Annulation…"}, {"Not processed", "Non traité"}, {"Conversion…", "Conversion…"},
        {"Processing %1 / %2", "Traitement %1 / %2"}, {"Queued", "En attente"},
        {"%1 completed · %2 failed%3", "%1 terminé(s) · %2 en échec%3"}, {" · queue stopped", " · file arrêtée"},
        {"%1 file(s)", "%1 fichier(s)"}, {"%1 files", "%1 fichiers"},
        {"No common format available. Check diagnostics.", "Aucun format commun. Consultez le diagnostic."},
        {"Choose a format, then start converting.", "Choisissez un format, puis lancez la conversion."},
        {"Backend diagnostics", "Diagnostic des moteurs"}, {"Your local conversion engines", "Vos moteurs de conversion locaux"},
        {"Cannot contact the Lorenz core.", "Impossible de joindre le moteur Lorenz."},
        {" · available", " · disponible"}, {" · not installed", " · non installé"},
        {"Codec support varies by installation. Missing engines disable their conversions. See the README for package details.",
         "Les codecs disponibles dépendent de l'installation. Un moteur absent désactive ses conversions. Voir le README pour les paquets."},
        {"Close", "Fermer"}, {"Cancelling. Close the window once processing has stopped.", "Annulation. Fermez la fenêtre une fois le traitement arrêté."},
        // Settings, tray and notifications
        {"Lorenz settings", "Réglages de Lorenz"}, {"Wheels", "Roues"},
        {"Start dragging a file, then tap a key. Each key opens its own wheel; tap another key to switch while it is open.",
         "Commencez à glisser un fichier, puis tapez une touche. Chaque touche ouvre sa roue ; tapez-en une autre pour changer."},
        {"Convert to another format", "Convertir dans un autre format"},
        {"Tools: compress, resize, rotate, split, merge…", "Outils : compresser, redimensionner, pivoter, séparer, fusionner…"},
        {"Actions: copy, move, share, terminal…", "Actions : copier, déplacer, partager, terminal…"}, {"Off", "Aucune"},
        {"Startup", "Démarrage"}, {"Start Lorenz when I log in", "Lancer Lorenz à l'ouverture de session"},
        {"The tray companion shows notifications and gives access to the batch manager. The KWin gesture works without it.",
         "L'icône près de l'horloge affiche les notifications et donne accès à la fenêtre de conversion. Le geste KWin fonctionne sans elle."},
        {"Language", "Langue"}, {"Automatic (system)", "Automatique (système)"},
        {"Save", "Enregistrer"}, {"Applies the next time Lorenz starts.", "S'applique au prochain démarrage de Lorenz."},
        {"Each wheel needs a different key.", "Chaque roue doit avoir sa propre touche."},
        {"Lorenz · drag a file, then tap Shift, Ctrl or Alt", "Lorenz · glissez un fichier, puis tapez Maj, Ctrl ou Alt"},
        {"Cancellation failed", "Échec de l'annulation"}, {"Conversion complete", "Conversion terminée"},
        {"Conversion failed", "Échec de la conversion"}, {"The tool failed", "L'outil a échoué"},
        {"Copy to", "Copier vers"}, {"Move to", "Déplacer vers"}, {"Action failed", "Échec de l'action"},
        {"Copied", "Copié"}, {"Ready to paste in your file manager.", "Prêt à coller dans votre gestionnaire de fichiers."},
        {"Path copied", "Chemin copié"}, {"%1 paths", "%1 chemins"}, {"Moved", "Déplacé"},
        {"%1 item(s) copied. Nothing was replaced.", "%1 élément(s) copié(s). Rien n'a été remplacé."},
        {"%1 item(s) moved. Nothing was replaced.", "%1 élément(s) déplacé(s). Rien n'a été remplacé."},
        {" created.", " créé."}, {"%1 items created.", "%1 éléments créés."}, {" Originals preserved.", " Originaux conservés."},
        {"Settings…", "Réglages…"}, {"Open batch manager", "Ouvrir la fenêtre de conversion"},
        {"Quit tray companion", "Quitter Lorenz"},
        {"Drag a file, then tap %1 to convert, %2 for tools or %3 for actions.",
         "Glissez un fichier, puis tapez %1 pour convertir, %2 pour les outils ou %3 pour les actions."}};
    return fr.value(english, english);
}
}
using lorenz::L;
