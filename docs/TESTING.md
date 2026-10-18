# Validation

## Automatisée

Les tests Rust vérifient le registre, les extensions, les erreurs JSON, le worker
JSON Lines, la préservation de la source, les collisions de destination concurrentes,
les chemins inhabituels, l'annulation, le délai et le nettoyage après erreur.
Les essais moteurs couvrent PNG → WebP, SRT → VTT et Markdown → HTML → texte.
Le lancement Qt hors écran vérifie la création de la fenêtre et sa fermeture.

## Recette manuelle par plateforme

1. Déposer deux PNG, vérifier les formats communs et choisir WebP avec la roue.
2. Convertir deux fois : les originaux restent inchangés et les sorties sont distinctes.
3. Déposer un fichier inconnu : aucun format disponible, message explicite.
4. Déposer un mélange de formats incompatibles : conversion désactivée.
5. Tester les noms Unicode, espaces, apostrophes et caractères de shell.
6. Choisir un autre dossier, convertir et ouvrir le dossier du résultat.
7. Lancer une vidéo longue, annuler, vérifier l'arrêt du moteur et l'absence de sortie partielle.
8. Fermer pendant un traitement : annulation demandée, fenêtre conservée jusqu'à l'arrêt.
9. Vérifier le diagnostic, les moteurs manquants et les erreurs de codecs.
10. Tester menus Nautilus/Dolphin/Nemo, Thunar et Envoyer vers Windows séparément.

À exécuter sous GNOME Wayland, KDE Wayland, Hyprland, Sway, XFCE X11, Windows 10
et Windows 11. Une compilation réussie ne valide pas ces interactions.
