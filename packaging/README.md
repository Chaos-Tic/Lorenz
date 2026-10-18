# Distribution

La version 0.1 utilise des installations utilisateur depuis les sources. Aucun paquet
n'est annoncé comme publié, signé ou accepté dans un dépôt de distribution.

- Linux : `scripts/install-linux.sh`, sans privilèges administrateur.
- Windows : `scripts/install-windows.ps1`, après installation de Rust, CMake, MSVC et Qt6.
- Le moteur et le bureau doivent être placés dans le même répertoire.
- Qt est lié dynamiquement ; les bibliothèques et plugins Qt doivent être distribués
  selon leur licence. Les moteurs restent optionnels et ne sont pas embarqués.
- Les artefacts CI du cœur ne comprennent ni Qt ni les moteurs de conversion.

Étapes avant une distribution publique : choisir la licence du projet, auditer les
licences des moteurs, produire et vérifier les paquets deb/rpm/PKGBUILD, déployer Qt
sous Windows et Linux, tester en VM, puis prévoir les signatures et les canaux de
mise à jour. Flatpak exige une stratégie explicite pour les moteurs hôtes et les
permissions ; aucune permission réseau ne doit être ajoutée par commodité.
