# Syntax languages / Langages de coloration

## English

The development sources recognize `.zig` and `.zon`, including `build.zig` and
`build.zig.zon`. Open them from the project tree, Files panel or file search.
These browsers accept ordinary files without a language-extension whitelist.
Passing a Zig/ZON file path to the desktop launcher also opens it as a document.
The solution picker remains for supported project formats; a Zig build file is
not a newly supported build-system entry point.

Click the language name in the status bar to open **Syntax language**. Choose
**Automatic (file association)** to follow the current CyUI/plugin associations,
or select **Zig**, **ZON** or another mode from the searchable shared catalogue.
A manual mode stays with that document through edits, rename/Save As and session
restoration. Older sessions keep automatic detection. If a selected plugin is
unavailable, its text identifier is retained and the view temporarily uses plain
text; the mode returns when the plugin becomes available again. Existing global
editor and language-server preferences are preserved.

The selected syntax also applies to search, recovery, refactoring and merge
previews, file diffs and paged views. Generated template previews use their own
file associations. Paged syntax uses a complete contextual snapshot up to 32 MiB,
cached by native document revision, language and catalogue revision. Only spans
intersecting the displayed fragment are supplied, clipped and rebased in UTF-8
bytes. Larger files retain paged editing without syntax highlighting. Multi-file patches
use plain text because the shared Diff API accepts one language per comparison;
file-specific comparisons use that file’s selected language.

This is offline syntax highlighting, **not complete Zig language support**.
It installs neither ZLS nor a Zig compiler, and adds no default clangd route for
Zig/ZON. Selecting a syntax mode changes presentation; semantic services still
require a separately configured compatible backend. No Core dependency on CyUI
is introduced.

## Français

Les sources de développement reconnaissent `.zig` et `.zon`, notamment
`build.zig` et `build.zig.zon`. Ouvrez-les depuis l’arbre du projet, le panneau
Fichiers ou la recherche de fichiers. Ces vues acceptent les fichiers ordinaires
sans liste restrictive d’extensions. Un chemin Zig/ZON passé au lanceur ouvre
également le fichier comme document. Le sélecteur de solution conserve les formats
de projet pris en charge ; un fichier de build Zig ne devient pas un nouveau
point d’entrée de compilation.

Cliquez sur le nom du langage dans la barre d’état pour ouvrir **Langage de
coloration**. Choisissez **Automatique (association du fichier)** pour suivre les
associations CyUI/plugins, ou **Zig**, **ZON** ou un autre mode du catalogue commun
filtrable. Un choix manuel suit le document après édition, renommage,
Enregistrer sous et restauration de session. Les anciennes sessions restent en
mode automatique. Si le plugin sélectionné est absent, son identifiant texte est
conservé et la vue passe temporairement en texte brut ; le mode revient avec le
plugin. Les préférences existantes de l’éditeur et des serveurs sont conservées.

Le langage choisi s’applique aussi aux aperçus de recherche, récupération,
refactorisation et fusion, aux comparaisons de fichiers et aux vues paginées.
Les aperçus des modèles générés utilisent leurs propres associations de fichiers.
La coloration paginée utilise un instantané contextualisé complet jusqu’à 32 Mio,
mis en cache par révision native, langage et révision du catalogue. Seules les
plages intersectant le fragment visible sont fournies, découpées et recalées en
octets UTF-8. Au-delà, l’édition paginée reste disponible sans coloration. Les patchs multi-fichiers
restent en texte brut : l’API Diff commune accepte un langage par comparaison.
Une comparaison ciblée utilise le langage choisi pour son fichier.

Il s’agit de coloration hors ligne, **pas d’un support sémantique Zig complet**.
ZLS et le compilateur Zig ne sont pas installés, et Zig/ZON ne sont pas envoyés
vers clangd par défaut. Le choix du langage règle la présentation ; les services
sémantiques demandent un backend compatible configuré séparément. Aucune
dépendance CyUI n’est ajoutée au Core.

## Local validation

Read [AGENTS.md](../AGENTS.md) before running anything. Production builds use
`BUILD_TESTING=OFF`. Python resolver/data checks may run without launching the
application. `app/tests/syntax_ui_tests.cpp` adds native regression scenarios to
the existing opt-in workflow fixture; it must remain unexecuted under the current
antivirus restriction. This fixture covers detection, manual/automatic modes,
legacy and plugin sessions, edits, Unicode, strings/comments, Undo/Redo, rename,
Diff caching and contextual paged byte ranges. Native runtime behavior and visual
theme/DPI/selection/scroll checks are not established by source checks alone.
