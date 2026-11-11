<table width="100%">
  <tr>
    <td width="300" align="left" valign="top">
      <a href="https://mrmybal.github.io/CyCode/Documentation.html#/fr/overview">Manuel</a><br>
      <a href="https://github.com/MrMybal/CyCodeIDE">Dépôt</a><br>
      <a href="VERSION.fr.md">Notes de version</a><br><br>
      <sub>README</sub><br>
      <a href="README.md">English</a> · <a href="README.fr.md">Français</a>
    </td>
    <td width="400" align="center" valign="middle">
      <img src="icons/CyCode.png" alt="CyCodeIDE" width="128">
    </td>
    <td width="300" align="right" valign="top">
      <p><sub>Licence de l’application</sub><br><a href="LICENSE"><strong>GPL-3.0-only</strong></a></p>
      <p><sub>Version de l’IDE</sub><br><a href="VERSION.fr.md"><strong>0.2.3</strong></a><br><strong>EN DÉVELOPPEMENT</strong></p>
    </td>
  </tr>
</table>

<h1 align="center">CyCodeIDE</h1>

Un IDE natif C++20 pour les projets de code, utilisant
[CyCode Core](https://github.com/MrMybal/CyCode) et
[l’éditeur partagé de CyCodeReader](https://github.com/MrMybal/CyCodeReader).

> **EN DÉVELOPPEMENT · CyCodeIDE 0.2.3**
>
> Cet état des sources provient de l’application de bureau CyCode 0.2.3.
> Il ne constitue ni un nouvel installateur ni une nouvelle publication.
> Consulter les [notes de version et limites documentées](VERSION.fr.md).

## À propos de CyCodeIDE

Modifier les fichiers d’un projet, consulter les diagnostics, naviguer entre les
symboles et examiner les actions de code dans une même interface de bureau.
L’analyse, l’autocomplétion et les transformations utilisent les services de Core
et dépendent du serveur et du projet configurés. Le
[manuel](https://mrmybal.github.io/CyCode/Documentation.html#/fr/overview)
explique la configuration et les opérations disponibles.

CyCodeIDE gère les fenêtres, panneaux de projet, thèmes, raccourcis et leur
présentation. Core gère les documents, adaptateurs de compilation, services de
langage persistants, index sémantiques, diagnostics, extensions et transactions
contrôlées. Reader fournit l’API source 1 de l’éditeur graphique. Les trois
partagent les composants natifs de tampon, encodage, processus et protocole,
avec un seul Core et une seule instance compatible ImGui/éditeur par compilation.
L’API source 1 ne garantit pas une ABI binaire C++ stable.

## Fonctionnalités

- **Édition du code** : éditeur partagé de Reader avec annulation/rétablissement,
  couleurs syntaxiques CyUI et JetBrains Mono embarquée. Ctrl + molette zoome
  indépendamment la vue de code survolée ; le bouton de pourcentage au-dessus
  d’un éditeur rétablit 100 %. Aperçus et comparaisons partagent la police de code
  et possèdent leur propre zoom à la molette.
- **Diagnostics et actions de code** : filtrer les problèmes, naviguer vers les
  emplacements, rechercher les actions et examiner les aperçus avant d’appliquer
  les modifications contrôlées par Core.
- **Outils de projet** : les panneaux de compilation, débogage et gestion de
  versions utilisent les services Core et les outils configurés pour le projet.
- **Espace de travail** : panneaux ancrables, fenêtres multiples, thèmes et
  raccourcis configurables.
- **Anglais et français** : l’anglais est la langue par défaut ; le français est
  sélectionnable. Le manuel bilingue CyDoc fonctionne aussi hors ligne.

---

## Installation

Cet état de développement se compile depuis les sources. L’identité de
l’installateur et des mises à jour ainsi que l’organisation de la distribution
doivent encore être migrées et validées avant une publication CyCodeIDE.
Les anciens téléchargements CyCode précèdent cette séparation.

La compilation cible Windows, Linux et macOS. Les [notes de version](VERSION.fr.md)
précisent les limites de validation ; une cible de compilation ne suffit pas à
valider l’interface de bureau complète ou l’installateur. L’application graphique
nécessite un pilote OpenGL.

### Lire le manuel

Consulter le [manuel en français](https://mrmybal.github.io/CyCode/Documentation.html#/fr/overview)
ou le [manuel anglais](https://mrmybal.github.io/CyCode/Documentation.html#/en/overview)
en ligne. Les compilations standard génèrent le véritable lecteur CyDoc hors
ligne depuis les sources Core verrouillées et le placent à côté de l’exécutable.

Ouvrir `app/Documentation/Documentation.html#/fr/overview` (ou `#/en/overview`)
depuis le répertoire de l’exécutable, ou utiliser la commande de documentation
de l’application. Conserver tout le dossier `app/Documentation/`, y compris
`DocData/`. Le lecteur exporté ne nécessite ni serveur, ni compte, ni connexion
Internet.

## Sources, compilation et bibliothèques nécessaires

### Prérequis

- **CMake 3.24+**, **Python 3.11+**, **Git** et une chaîne de compilation **C++20**.
- **Windows** : outils C++ x64 de Visual Studio 2022.
- **Linux/macOS** : chaîne C++ adaptée, Ninja et bibliothèques de développement OpenGL de la plateforme.
- **Node.js 22+ et npm** pour la génération de documentation activée par défaut.
- Accès aux dépôts et paquets pour le premier téléchargement des dépendances.

### Récupérer les sources et compiler

```sh
git clone https://github.com/MrMybal/CyCodeIDE.git
cd CyCodeIDE
python app/scripts/build.py --fetch
```

CyCodeIDE nécessite actuellement un accès au dépôt. Core et Reader sont publics.
L’authentification Git disponible sert si nécessaire ; les identifiants n’ont
pas leur place dans les sources ou les exemples de commandes.

Le script compile les cibles de production avec `BUILD_TESTING=OFF` et ne lance
ni l’application ni les tests natifs. Sortie par défaut :
`runtime/build/app/Release/CyCodeIDE.exe` sous Windows, ou
`runtime/build/app/CyCodeIDE` avec un générateur à configuration unique.
La cible interne de l’exécutable s’appelle encore `CyCode`.

<details>
<summary>Répertoires de compilation séparés et dépendances de développement</summary>

Conserver une autre configuration sans remplacer une compilation existante :

```sh
python app/scripts/build.py --build-dir runtime/build-dev --fetch
```

Utiliser des sources de développement explicites pour coordonner Core et Reader :

```sh
python app/scripts/build.py --core-root <CyCode> --editor-root <CyCodeReader> --fetch
```

Les révisions exactes sont verrouillées dans les fichiers
[Core](app/core-source.lock.json), [Reader](app/editor-source.lock.json) et
[CyUI](app/cyui-source.lock.json). Les dépendances natives et celles de l’éditeur
suivent leurs verrouillages respectifs ; les [dépendances d’images](app/dependencies.lock.json)
sont verrouillées ici.

Le manuel hors ligne est généré par défaut. `--without-documentation` est une
option explicite réservée à la vérification de compilation. La préparation
reprend la façade, le manifeste et la CLI canoniques de Core sans créer un second
registre d’opérations. Elle exclut les espaces de travail locaux, paramètres
utilisateur et données de tâches téléchargées.

</details>

---

## Licence

**CyCodeIDE utilise [GNU GPL v3 uniquement](LICENSE) (`GPL-3.0-only`).**
Voir [COPYRIGHT](COPYRIGHT). Les dépendances conservent leurs licences et notices.

### Composants natifs et interface

| Bibliothèque · sources | Licence · texte | Utilisation / périmètre |
| --- | --- | --- |
| [CyCode Core](https://github.com/MrMybal/CyCode) | [GPL-3.0-only](https://github.com/MrMybal/CyCode/blob/5dc05f4bffe5d38e871250c9de23651c33d47221/LICENSE) | Services natifs de projet, documents et CLI partagés. |
| [Éditeur CyCodeReader](https://github.com/MrMybal/CyCodeReader) | [GPL-3.0-only](https://github.com/MrMybal/CyCodeReader/blob/d27e84f1097b68c12630dc4c10fcc406d4628760/LICENSE) | API source 1 de l’éditeur graphique partagé. |
| [CyUI](https://github.com/MrMybal/CyUI) | [MIT](https://github.com/MrMybal/CyUI/blob/6173630ad5eb1e585eee1a0e7d3617285563e774/LICENSE) | Modules sélectionnés d’interface, code, comparaison et panneaux. |
| [Dear ImGui](https://github.com/ocornut/imgui) | [MIT](https://github.com/ocornut/imgui/blob/d682dc41f58b98c9343ffbaa7d94f07e9024b3ca/LICENSE.txt) | Interface native et adaptateurs SDL/OpenGL partagés. |
| [SDL3](https://github.com/libsdl-org/SDL) | [zlib](https://github.com/libsdl-org/SDL/blob/release-3.4.18/LICENSE.txt) | Fenêtres natives, saisie et intégration à la plateforme. |
| [ImGuiColorTextEdit](https://github.com/goossens/ImGuiColorTextEdit) | [MIT](https://github.com/MrMybal/CyUI/blob/6173630ad5eb1e585eee1a0e7d3617285563e774/third_party/texteditor/LICENSE) | Éditeur CyUI ; Reader adapte des copies propres à la compilation. |
| [Lexilla / Scintilla](https://github.com/ScintillaOrg/lexilla) et [propriétés SciTE](https://github.com/mirror/scite) | [Notices permissives des composants](https://github.com/MrMybal/CyUI/blob/6173630ad5eb1e585eee1a0e7d3617285563e774/THIRD_PARTY_NOTICES.md) | Analyse lexicale, en-têtes publics et données de mots-clés ; sans interface d’éditeur Scintilla. |
| [Tree-sitter et grammaires](https://github.com/tree-sitter) | [MIT ; notices des composants](app/THIRD_PARTY_NOTICES.md) | Analyse syntaxique de l’éditeur partagé. |
| [stb_image](https://github.com/nothings/stb) | [MIT ou Unlicense](app/THIRD_PARTY_NOTICES.md) | Décodage d’images. |
| [libwebp](https://github.com/webmproject/libwebp) | [Licence de type BSD et concession de brevets](app/THIRD_PARTY_NOTICES.md) | Décodage d’images uniquement. |
| [PCRE2](https://github.com/PCRE2Project/pcre2) | [BSD-3-Clause avec exception PCRE2](app/THIRD_PARTY_NOTICES.md) | Recherche UTF-8 dans les extraits de code de Core. |
| [utf8proc](https://github.com/JuliaStrings/utf8proc) | [MIT et conditions des données Unicode](app/THIRD_PARTY_NOTICES.md) | Conversion de casse Unicode des extraits de Core. |
| [Monocypher](https://monocypher.org) | [BSD-2-Clause ou CC0-1.0](app/THIRD_PARTY_NOTICES.md) | Vérification des signatures de paquets par Core. |
| [TinyXML2](https://github.com/leethomason/tinyxml2) | [zlib](app/THIRD_PARTY_NOTICES.md) | Données XML des projets Core. |
| [libvterm](https://www.leonerd.org.uk/code/libvterm/) | [MIT](app/THIRD_PARTY_NOTICES.md) | Émulation du terminal natif. |
| [nlohmann/json](https://github.com/nlohmann/json) | [MIT](app/THIRD_PARTY_NOTICES.md) | Données et protocoles JSON natifs. |
| [JetBrains Mono 2.304](https://github.com/JetBrains/JetBrainsMono) | [SIL OFL 1.1](app/resources/fonts/JetBrainsMono-OFL.txt) | Police de code embarquée et police de repli hors ligne. |
| [Roboto](https://github.com/googlefonts/roboto-2) | [Apache-2.0](app/resources/fonts/Roboto-LICENSE.txt) | Police de repli hors ligne pour l’interface. |
| [Microsoft Codicons 0.0.36](https://github.com/microsoft/vscode-codicons) | [CC BY 4.0](app/resources/fonts/Codicons-LICENSE.txt) | Police d’icônes CyCode Controls adaptée et renommée. |

### Documentation et outils optionnels

| Bibliothèque · sources | Licence · texte | Utilisation / périmètre |
| --- | --- | --- |
| [CyDoc 0.1.0](https://github.com/MrMybal/CyDoc) | [MIT](https://github.com/MrMybal/CyDoc/blob/6ddf9a582defb2f11fbb3f992afef6f98dcadc93/LICENSE) | Lecteur hors ligne distribué avec les modules image et galerie. |
| [marked 17.0.6](https://github.com/markedjs/marked) | [MIT](https://github.com/markedjs/marked/blob/v17.0.6/LICENSE.md) | Compilation Markdown pendant la génération. |
| [PrismJS 1.30.0](https://github.com/PrismJS/prism) | [MIT](https://github.com/PrismJS/prism/blob/v1.30.0/LICENSE) | Coloration du code de la documentation pendant la génération. |
| [esbuild 0.25.12](https://github.com/evanw/esbuild) | [MIT](https://github.com/evanw/esbuild/blob/v0.25.12/LICENSE.md) | Assemblage de la documentation pendant la génération. |

Serveurs de langage, compilateurs, adaptateurs de débogage et environnements
SDK/MCP optionnels conservent les conditions de leurs composants. Ce sont des
prérequis distincts ou des paquets sélectionnés, pas des fonctions livrées
systématiquement. Les [notices tierces](app/THIRD_PARTY_NOTICES.md) précisent leur
périmètre.

Ces tableaux recensent les principaux composants sans remplacer leurs notices
transitives complètes. Conserver les textes de licence distribués, les notices
des polices et `Documentation/DocData/licenses/` avec leurs composants.

## Références et liens

- [Manuel en français](https://mrmybal.github.io/CyCode/Documentation.html#/fr/overview) · [English manual](https://mrmybal.github.io/CyCode/Documentation.html#/en/overview).
- [Dépôt des sources](https://github.com/MrMybal/CyCodeIDE) · [Notes de version et limites](VERSION.fr.md).
- [CyCode Core](https://github.com/MrMybal/CyCode) · [CyCodeReader](https://github.com/MrMybal/CyCodeReader) · [CyUI](https://github.com/MrMybal/CyUI).

<details>
<summary><strong>AI Usage</strong></summary>

- [Commencer ici : instructions du dépôt](AGENTS.md) — périmètre, guides importants et restrictions de développement.
- [Développer l’interface](app/AGENTS.md) — présentation CyUI et responsabilités des services partagés.
- [Intégrer Core](https://github.com/MrMybal/CyCode/blob/5dc05f4bffe5d38e871250c9de23651c33d47221/app/Docs/CoreExtraction.md) — architecture native verrouillée et contrats source.

</details>
