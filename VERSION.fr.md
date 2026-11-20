# Notes de version de CyCodeIDE

[README](README.fr.md) · [English](VERSION.md)

## CyCodeIDE 0.2.5 · Windows x64 · 2026-11-20

**En développement.** Le MCP local utilise Free par défaut, avec connexion par URL
et conservation des choix d’accès explicites. [Téléchargements](https://github.com/MrMybal/CyCodeIDE/releases/tag/v0.2.5)
· [Notes de publication](Docs/Releases/v0.2.5.fr.md) · [Version précédente](Docs/Releases/v0.2.4.fr.md).

## Changements

- **Free par défaut :** connecter un client MCP externe avec l’URL locale seule,
  sans jeton ni confirmation supplémentaire par opération. Schémas d’arguments,
  révisions, limites du projet et sauvegardes explicites restent appliqués.
- **Quatre profils d’accès :** Free, Local contrôlé, Jeton seul et Protégé.
  Protégé exige toujours un jeton et les droits de l’hôte. Le MCP local peut être désactivé.
- **Migration et persistance :** les choix absents migrent vers Free. Profils
  explicites, alias SDK et anciens choix jeton/contrôle conservent leur comportement.
  Enregistrer les préférences de l’éditeur conserve toute la configuration MCP.
- **Panneau MCP :** copier l’adresse, la configuration HTTP directe ou la
  configuration CLI stdio existante. Les en-têtes à jeton ne sont ajoutés que pour les profils concernés.
- **Services partagés :** chat, CLI et MCP externe utilisent toujours le moteur
  Core de la fenêtre, ses documents ouverts et transactions contrôlées. Changer de
  transport déconnecte le chat ; reconnecter explicitement. Les documents restent ouverts.
- Core 0.1.6 et CyMCPSDK 0.7.2 compilés depuis les sources locales. Voir le
  [guide de l’assistant et de l’automatisation](Docs/AssistantMCP.fr.md).

## Composants

| Composant | Version / sources sélectionnées |
| --- | --- |
| CyCodeIDE | 0.2.5 |
| [CyCode Core](app/core-source.lock.json) | 0.1.6 · `b902736b0edb9a76365a47778af8a223d50e5b83` · API source 1 |
| [Éditeur Reader](app/editor-source.lock.json) | `889dd5fd8db8bfabdba4601f5d2a955723c91301` · API graphique source 1 |
| [CyUI](app/cyui-source.lock.json) | `6ef01cfcdb197f3a89d64618ce5ef8818c704728` |
| CyMCPSDK | 0.7.2 · `e562c731a146d599e3404718ea8414bb5b96b66d` (inclut `34d4bca`) |

L’API source 1 ne garantit pas une ABI C++ stable. Compiler ces composants ensemble.

## Validation et limites documentées

La compilation Windows x64 Release et dix vérifications Python de staging/contrats
ont réussi. Quatorze vérifications isolées de production utilisent le vrai IDE
avec un fournisseur local simulé : quatre profils MCP, lectures/modifications sans
jeton, refus de jeton/droits, validation des arguments, migration/persistance,
CLI, chat et annulation/restauration. Gitleaks et contrôles du paquet/empreintes/liens
ont réussi. Cela ne valide ni les comptes fournisseurs réels ni les dialogues interactifs de pièces jointes.

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
