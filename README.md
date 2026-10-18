# Metroid

Application native de conversion de fichiers pour Linux et Windows, avec un cœur
Rust et un bureau Qt6. Les originaux sont conservés et les conversions s'exécutent
sur l'ordinateur. Aucun compte, serveur applicatif ou service cloud n'est requis.

**État : version 0.1, en développement.** Ce projet est une nouvelle implémentation ;
il ne contient pas le code de Tangerine et n'est affilié à aucune franchise homonyme.

## Utilisation

Ouvrir **Metroid** dans le menu des applications, déposer des fichiers, choisir un
format et cliquer sur **Convertir**. La liste n'affiche que les formats communs aux
fichiers sélectionnés dont le moteur est installé. Le bouton rond ouvre la roue.
`Ctrl+O` ajoute des fichiers ; `Échap` demande l'annulation du traitement en cours.
Le diagnostic indique les moteurs présents. Les erreurs détaillées apparaissent
au survol de l'état du fichier.

Les sorties sont nommées `nom-metroid.ext`, puis `nom-metroid-2.ext`, etc. Une
conversion par fichier s'exécute à la fois. Une erreur n'interrompt pas le reste du
lot ; une annulation l'interrompt. Les chemins ne passent jamais par un shell.

## Installation Linux

Prérequis : Rust stable (édition 2024), compilateur C++, CMake ≥ 3.21, Qt6 Widgets ≥ 6.4.

```sh
./scripts/install-linux.sh
```

L'installation ajoute le bureau, le cœur, l'icône et les intégrations utilisateur
Nautilus, Dolphin et Nemo. Elle ne lance aucun service en arrière-plan. Les moteurs
sont optionnels : ImageMagick 7 (`magick`), FFmpeg, Pandoc, Poppler et qpdf. Installez
les paquets de votre distribution. Le diagnostic ne prouve pas la présence de tous
les codecs : HEIC, AVIF et H.264 dépendent notamment de la compilation des moteurs.

Sous Wayland, le bureau utilise une fenêtre ordinaire ; le maintien au premier plan
dépend du compositeur. Aucun glisser-déposer global ni frappe globale n'est capturé.
Pour Thunar, créer une action utilisateur `metroid gui %F`. Pour un bureau tiling,
associer un raccourci à `metroid gui` ou passer un fichier à `metroid pick`.

## Installation Windows

Installer Rust, les outils C++ MSVC, CMake et Qt6 MSVC. Placer Qt dans
`CMAKE_PREFIX_PATH` et ses outils dans `PATH`, puis exécuter :

```powershell
./scripts/install-windows.ps1
```

L'installation utilisateur ajoute Metroid au menu Démarrer et à « Envoyer vers ».
Les moteurs doivent être installés séparément et présents dans `PATH`. La compilation
Windows est configurée en CI ; la validation interactive Windows 10/11 reste à faire.

## Conversions implémentées

| Famille | Moteur | Portée |
| --- | --- | --- |
| Images | ImageMagick 7 | PNG, JPG, WebP, TIFF, BMP, AVIF, PDF ; HEIC en entrée |
| Audio | FFmpeg | MP3, M4A, WAV, FLAC, OGG, Opus, AIFF ; extraction depuis vidéo |
| Vidéo | FFmpeg | MP4, MOV, MKV, WebM, AVI, GIF |
| Documents | Pandoc | MD, DOCX, EPUB, HTML, ODT, TXT ; RTF en entrée |
| PDF | Poppler | Texte et PNG de la première page à 150 dpi |
| Sous-titres | FFmpeg | SRT ↔ VTT |
| Fusion PDF | qpdf | Commande CLI `merge-pdf` |

Le registre se trouve dans `registry/conversions.toml` et est embarqué à la
compilation. Ce sont des routes déclarées, pas une certification de toutes les
combinaisons et de tous les codecs. La détection actuelle repose sur l'extension ;
le moteur valide le contenu. Les images multipages peuvent produire des fichiers
supplémentaires non publiés : cette version vise les images monopages.

Pandoc est lancé avec `--sandbox`. Certaines distributions de Pandoc ne peuvent pas
produire DOCX/ODT dans ce mode si leurs données ne sont pas embarquées. Metroid
conserve cette protection et remonte l'erreur ; utiliser une distribution Pandoc
compilée avec `embed_data_files`. Voir le [manuel Pandoc](https://pandoc.org/MANUAL.html#general-options).
Les ressources externes ne sont pas automatiquement récupérées. Les moteurs externes
ne sont pas isolés par un bac à sable système : le traitement local ne constitue pas
une garantie réseau pour tous leurs délégués, ni une protection contre leurs failles.

## Ligne de commande

```sh
metroid doctor
metroid formats photo.png --json
metroid convert photo.png --to webp
metroid convert film.mov --to mp4 --output-dir ./exports
metroid merge-pdf chapitre1.pdf chapitre2.pdf --output-dir ./exports
metroid pick photo.png --menu fuzzel
metroid gui photo.png
```

`convert --json` produit une ligne JSON par fichier, avec `ok`, `input` et `output`
ou `error`. Le code de sortie vaut 1 si au moins une conversion échoue.
`--timeout` fixe la limite en secondes (3600 par défaut). `--cancel-file` indique un
fichier signal : son apparition provoque l'arrêt du moteur et le nettoyage temporaire.
`serve` est un worker séquentiel JSON Lines sur stdin/stdout, sans socket réseau :

```json
{"input":"/chemin/photo.png","to":"webp","output_dir":"/chemin/sorties"}
```

Un arrêt normal nettoie les fichiers de travail. Une coupure brutale peut laisser un
dossier `.metroid-*` près des sorties ; il peut être supprimé après vérification
qu'aucune conversion n'est en cours. La publication atomique utilise les liens physiques :
un volume qui ne les prend pas en charge (par exemple FAT/exFAT) renvoie une erreur.
La file du bureau n'est pas persistée entre deux lancements.

## Développement et vérification

```sh
cargo fmt --check
cargo clippy --locked --all-targets -- -D warnings
cargo test --locked
cargo build --release --locked
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
QT_QPA_PLATFORM=offscreen build/metroid-desktop --smoke-test
```

Les tests utilisent les moteurs installés ; ceux absents sont explicitement sautés.
La CI configure Linux/Windows pour Rust et Qt, ainsi que Debian stable/testing,
Arch et Fedora stable/rawhide pour le cœur. Les interactions propres à chaque bureau
nécessitent aussi une validation manuelle, décrite dans `docs/TESTING.md`.

## Suite du projet

- Détection MIME/contenu, capacités et codecs détaillés, progression par moteur.
- Annulation des arbres de processus et limites de ressources système.
- Démon persistant, notifications et historique local configurable.
- Extraction d'archives sécurisée, Office/LibreOffice, tableaux et PDF multipages.
- Compression, redimensionnement, rognage et découpe avec paramètres graphiques.
- Portails XDG, layer-shell optionnel et raccourcis globaux selon le bureau.
- Validation des menus contextuels dans les bureaux cibles et intégration Windows 11.
- Paquets natifs, Flatpak/AppImage, installeur Windows signé et licence de publication.

Voir `packaging/README.md` pour la distribution. Aucune licence de redistribution
n'est encore accordée ; le choix reste à arrêter avant une publication publique.
