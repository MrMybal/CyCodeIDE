# Notes de version de CyCodeIDE

[README](README.fr.md) · [English](VERSION.md)

## Version actuelle des sources

**CyCodeIDE 0.2.3 · EN DÉVELOPPEMENT**

Il s’agit de la version de l’IDE déclarée par les projets CMake racine et
application, indépendante du moteur Core lié. La présentation provient de
l’ancienne application de bureau CyCode 0.2.3. Ces notes décrivent les sources
actuelles, pas un nouveau binaire, un installateur ou une date de publication.

| Composant | Sources sélectionnées / contrat |
| --- | --- |
| CyCodeIDE | Sources de développement 0.2.3 |
| [CyCode Core](app/core-source.lock.json) | `5dc05f4bffe5d38e871250c9de23651c33d47221` · API source native/commune 1 · `cycode-core/1` |
| [Éditeur Reader](app/editor-source.lock.json) | `d27e84f1097b68c12630dc4c10fcc406d4628760` · API source de l’éditeur graphique 1 |
| [CyUI](app/cyui-source.lock.json) | `6173630ad5eb1e585eee1a0e7d3617285563e774` |
| Lecteur CyDoc | 0.1.0 · issu du verrouillage de documentation de Core |

L’API source 1 n’est pas une ABI binaire C++ stable. L’IDE, Core et Reader doivent
partager un graphe de dépendances compatible.

## Changements dans cet état des sources

- CyCodeIDE gère la présentation ; Core conserve les services natifs et
  transactions contrôlées ; Reader fournit l’éditeur graphique partagé.
- Le délai maximal de compilation utilise l’édition entière bornée et le cycle
  valider/annuler de l’inspecteur CyUI, avec messages de validation anglais/français.
- La navigation des diagnostics suit les déplacements manuels du curseur.
  Les filtres distinguent un résultat vide de l’absence de diagnostics et
  conservent l’identité de la sélection.
- La recherche des actions conserve leurs identités Core et contrôles de révision.
  Recherche et fenêtres contextuelles gardent leur saisie clavier ; l’aperçu
  conserve son intention.
- CyUI fournit JetBrains Mono 2.304 embarquée, les couleurs syntaxiques actualisées
  et un zoom Ctrl + molette indépendant. Le bouton de pourcentage de chaque
  éditeur rétablit 100 %.
- `--build-dir` choisit un répertoire de compilation de production séparé.
  Les compilations standard incluent le manuel bilingue hors ligne et ne lancent
  aucun binaire de l’application ou des tests.
- Les README anglais et français complets suivent la convention CyDoc, avec
  notes de version et références aux licences des dépendances.

## Limites documentées

- L’identité de l’installateur et des mises à jour ainsi que l’organisation de la
  distribution doivent être migrées et validées avant la première publication
  CyCodeIDE. L’ancien système de mise à jour est désactivé ; son auxiliaire n’est
  ni compilé ni préparé. L’identité et la version de l’IDE sont indépendantes de Core.
- Les sources actuelles ont été compilées avec succès en production Windows
  Release. Les résultats CI natifs et d’interface antérieurs concernent des
  révisions précises sous Windows, Linux et macOS. Ils ne suffisent pas à valider
  l’interface complète, le multi-écran ou l’installateur de cet état des sources.
  Le [guide d’extraction](Docs/Extraction.md) conserve les preuves exactes et
  vérifications non exécutées.
- L’exécution native locale reste suspendue pendant le diagnostic antivirus en
  cours. GitHub Actions reste en pause à la demande du propriétaire. Les
  compilations ordinaires utilisent `BUILD_TESTING=OFF` ; les récents tests de
  régression natifs n’ont pas été exécutés localement.
- Autocomplétion, diagnostics et actions dépendent des serveurs de langage
  configurés et des capacités prises en charge par Core. La coloration seule
  n’effectue pas d’analyse sémantique. Consulter les
  [limites du manuel](https://mrmybal.github.io/CyCode/Documentation.html#/fr/limits).
- La distribution publique de binaires nécessite encore un audit complet des
  notices et licences transitives des paquets langage/débogage/SDK sélectionnés.
  Conserver les [notices tierces](app/THIRD_PARTY_NOTICES.md).

## Problèmes, retours et références de publication

Ces notes ne tiennent pas de liste détaillée des bogues de cet état de
développement ; cela ne signifie pas qu’il en est exempt. Signaler les problèmes de l’IDE depuis
le [dépôt CyCodeIDE](https://github.com/MrMybal/CyCodeIDE), qui nécessite
actuellement un accès. Pour le moteur partagé, utiliser le
[suivi des problèmes de CyCode Core](https://github.com/MrMybal/CyCode/issues).

Les [anciennes publications CyCode](https://github.com/MrMybal/CyCode/releases)
concernent l’ancienne application, pas cet état extrait de CyCodeIDE.
