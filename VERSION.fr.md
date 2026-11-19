# Notes de version de CyCodeIDE

[README](README.fr.md) · [English](VERSION.md)

## CyCodeIDE 0.2.4 · Windows x64 · 2026-11-20

**En développement.** Cette version ajoute l’assistant intégré et l’automatisation
native à l’IDE existant. [Téléchargements](https://github.com/MrMybal/CyCodeIDE/releases/tag/v0.2.4)
· [Notes de publication](Docs/Releases/v0.2.4.fr.md) · [Version précédente](Docs/Releases/v0.2.3.fr.md).

## Changements

- **Affichage → Assistant** ouvre le chat natif : choix du fournisseur/modèle,
  réponses progressives, annulation, pièces jointes et restauration des conversations privées.
- Chat, CLI et MCP externe partagent les services Core de la fenêtre, ses documents
  ouverts, révisions et transactions contrôlées. L’accès est en lecture seule par
  défaut ; édition/compilation et préférences nécessitent des autorisations distinctes.
- Huit outils IDE exposent les onglets, la sélection, les panneaux pris en charge
  et les préférences communes. Un document modifié ne peut pas être fermé avec abandon implicite.
- CyMCPSDK remplace CyToolsCore et le serveur MCP Python. Les commandes CLI
  existantes et la migration explicite des anciennes tâches restent disponibles.
  Voir le [guide de l’assistant et de l’automatisation](Docs/AssistantMCP.fr.md).
- Cet état des sources inclut aussi les améliorations récentes de l’arborescence,
  des menus, de la recherche, du zoom partagé, de la minicarte et de la gouttière des points d’arrêt.

## Composants

| Composant | Version / sources sélectionnées |
| --- | --- |
| CyCodeIDE | 0.2.4 |
| [CyCode Core](app/core-source.lock.json) | 0.1.5 · `5da07d2256b6035ee26425987e6238b647fe8334` · API source 1 |
| [Éditeur Reader](app/editor-source.lock.json) | `889dd5fd8db8bfabdba4601f5d2a955723c91301` · API graphique source 1 |
| [CyUI](app/cyui-source.lock.json) | `6ef01cfcdb197f3a89d64618ce5ef8818c704728` |
| CyMCPSDK | 0.7.0 · `791d4d7d7680213b282ec8529d248a10f8494d77` |

L’API source 1 ne garantit pas une ABI C++ stable. Compiler ces composants ensemble.

## Validation et limites documentées

La compilation Windows x64 Release et dix vérifications Python de staging/contrats
ont réussi. Les essais isolés CLI/MCP/chat/interface masquée de production ont
réussi avec un fournisseur HTTP simulé local explicitement autorisé : vrais appels
d’outils IDE, droits, modifications non enregistrées, sauvegarde explicite,
annulation/restauration et lecture Unicode complète par morceaux. Cela ne valide
ni les comptes fournisseurs réels ni les dialogues interactifs de pièces jointes.

- Exécution de l’installateur, machine propre, remplacement/redémarrage réels en
  ligne, Linux/macOS et interactions complètes restent non vérifiés. Les binaires ne sont pas signés.
- Serveurs de langage, compilateurs, clients VCS et pilote OpenGL restent des
  prérequis externes. La coloration seule ne prouve pas l’analyse sémantique.
- Les tampons GUI/LSP ordinaires gardent leur limite de 4 Mio. Le profil UTF-8
  paginé n’a ni superposition LSP, conversion d’encodage, pliage, multi-curseur ni
  snippets ; enregistrer ou fermer ses modifications avant les requêtes au compilateur.
  La coloration avec contexte reste indisponible au-delà de 32 Mio.
- L’historique natif conserve jusqu’à 256 transactions. Fermer ou réaffecter un
  document invalide ses entrées liées au chemin. Les points de reprise paginés restent distincts d’Enregistrer.
- Relais MCP d’applications tierces et supervision d’un moteur local d’inférence
  ne sont pas inclus. Les droits natifs fichiers/terminal du fournisseur restent distincts des outils IDE.
- GitHub Actions reste en pause. Les compilations normales gardent `BUILD_TESTING=OFF` ;
  les essais isolés de production autorisés ne lèvent pas les restrictions générales d’exécution locale.

Consulter le [guide des mises à jour](Docs/Updates.md) et conserver les
[notices tierces](app/THIRD_PARTY_NOTICES.md). Aucun inventaire bogue par bogue
n’est tenu pour cet état ; signaler les problèmes au [suivi IDE](https://github.com/MrMybal/CyCodeIDE/issues)
ou au [suivi Core](https://github.com/MrMybal/CyCode/issues) pour les services partagés.
