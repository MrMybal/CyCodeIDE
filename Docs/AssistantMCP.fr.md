# Assistant et automatisation native

Ouvrez **Affichage → Assistant**, ou **Ouvrir l’assistant** dans le panneau MCP.
Le chat natif CyMCPSDK utilise le moteur Core de cette fenêtre : documents ouverts,
modifications non enregistrées, index du projet et transactions restent dans ce moteur.

Choisissez **Détecter les fournisseurs** ou sélectionnez un fournisseur et son
véritable exécutable/service. La clé API vient de la variable d’environnement
indiquée ; elle n’est jamais enregistrée dans les préférences. Les API distantes
demandent un modèle de leur catalogue réel. **Connecter / Nouvelle conversation**
n’envoie aucun message. Le panneau du SDK gère la réception progressive, l’annulation,
les erreurs, les pièces jointes et le choix du modèle. **Restaurer la conversation**
restaure le point de reprise privé ou reconnecte exactement la session native ;
une session manquante n’est pas remplacée silencieusement.

L’accès est initialement en lecture seule. **Enable editing, builds and plugin
commands** — traduit dans l’interface — accorde les droits existants du Core.
**Autoriser la modification des préférences de l’éditeur** accorde séparément
`settings.write`. Chaque appel vérifie ces droits, y compris depuis un client externe.
Les droits natifs du fournisseur sur les fichiers et terminaux restent distincts,
refusés par défaut. Aucun mode agent sans restriction n’est sélectionné.

## CLI et MCP externe

Le CLI du Core et ses anciennes commandes sont conservés. L’exécutable IDE accepte
aussi `--sdk` avant toute création de fenêtre SDL. Pour piloter une fenêtre ouverte,
utilisez le descripteur privé indiqué par **Copier la configuration MCP** :

```text
CyCodeIDE.exe --sdk --help
cycode_cli.exe --sdk --bridge-config <descripteur-prive.json> list-operations
cycode_cli.exe --sdk --bridge-config <descripteur-prive.json> describe-operation ide_state
cycode_cli.exe --sdk --bridge-config <descripteur-prive.json> invoke ide_state --params-file requete.json
cycode_cli.exe --sdk --bridge-config <descripteur-prive.json> --access edit invoke ide_open_document --params-file ouvrir.json
```

`requete.json` peut contenir `{ "offset": 0, "limit": 50 }` ; `ouvrir.json` peut
contenir `{ "path": "src/main.cpp", "line": 0, "character": 0 }`. Les positions
sont en UTF-16, indexées à partir de zéro. stdout reçoit le JSON, stderr les erreurs ;
un échec renvoie un code de sortie non nul. Le CLI du Core fonctionne aussi sans
interface sur un projet explicitement choisi. Les opérations `ide_*` exigent une
instance IDE ouverte.

La configuration MCP lance le CLI natif en stdio et le raccorde au point d’accès
local authentifié de cette fenêtre. Les descripteurs et jetons restent privés.
La fermeture invalide l’accès ; récupérez une nouvelle configuration après réouverture.
Le profil compact expose cinq outils du SDK : rechercher une opération, charger
son schéma, l’appeler, puis lire/rechercher ses ressources. `--mcp-profile direct-full`
conserve la découverte complète. Les contrats du Core sont préservés ; l’hôte ajoute :

| Opération | Fonction | Permission |
| --- | --- | --- |
| `ide_state` | Document courant, onglets paginés, panneaux et préférences | Lecture |
| `ide_commands` | Catalogue paginé des commandes et raccourcis, sans exécution arbitraire | Lecture |
| `ide_selection` | Sélection de l’éditeur et révision observée | Lecture |
| `ide_open_document` | Ouvrir un document du projet et montrer une position | `workspace.write` |
| `ide_activate_document` | Activer un document ouvert | `workspace.write` |
| `ide_close_document` | Fermer un onglet propre à sa révision attendue | `workspace.write` |
| `ide_show_panel` | Afficher un panneau pris en charge | `workspace.write` |
| `ide_editor_preferences` | Zoom global du code, taille et minicarte | `settings.write` |

La fermeture de documents modifiés, leur abandon, l’exécution de commandes
arbitraires, la fermeture de l’application et les changements de racines ou de
secrets sont exclus. Les contrats Core existants gèrent les modifications,
aperçus, applications, annulations et sauvegardes. Les actions UI passent par le
thread d’affichage ; le Core conserve les tâches persistantes et leur annulation.
Une mutation n’est jamais mise en cache ni rejouée automatiquement.

## Historique et options indépendantes

Les tâches terminées et conversations sont privées, sous
`data/automation/<projet>/slots/`. Chaque fenêtre réserve un emplacement distinct ;
les emplacements libres sont réutilisés après redémarrage. L’expiration d’une
ressource SDK n’efface pas les résultats durables du Core. Le travail interrompu
n’est pas rejoué. Les anciennes bases restent intactes :
`app/scripts/migrate_legacy_jobs.py --state-dir <ancien-etat>` importe explicitement
un état inactif, ensuite accessible avec `--data-dir` dans le CLI natif. Le guide
`app/Docs/NativeAutomation.md` du Core détaille la récupération et les propriétaires.

L’éditeur de connexion règle séparément la découverte différée et la fenêtre de
contexte API. Celle-ci est mesurée en octets, conserve l’historique complet et les
groupes appels/résultats ; les agents CLI gèrent leur propre contexte. Aucun
résumé automatique n’est activé.

La configuration hôte de confiance `automation` règle séparément `inlineBytes`,
`resultTtlSeconds`, `cacheTtlMs`, `diskResults`, `notifications` et `telemetry`.
Le stockage temporaire sur disque, les notifications et les mesures JSONL sans
contenu sont désactivés par défaut. Les ressources se lisent par morceaux bornés
avant expiration. Retrouvez le reçu durable d’une mutation plutôt que de la répéter.
Les abonnements dépendent du transport ; TTL et rafraîchissement explicite restent
disponibles. La recherche utilise l’index du Core et son repli borné existant,
sans seconde racine de recherche SDK. Le relais des outils MCP d’applications tierces n’est pas inclus dans cette
intégration. Le module de supervision d’un moteur local d’inférence est désactivé.

## Compilation depuis les sources

`app/core-source.lock.json` sélectionne le Core. Son verrou SDK fixe CyMCPSDK
**0.7.0**, révision `791d4d7d7680213b282ec8529d248a10f8494d77`.
Les modules Core, Platform, Http, MCP, AI et ChatUI sont compilés depuis des sources
locales propres. Local, exemples et tests SDK sont exclus du graphe de production.
Le code SDK reste inchangé ; CyUI et Reader partagent un seul ImGui/backend.

```text
python app/scripts/build.py --core-root <Core-propre> --cymcp-source <CyMCPSDK-propre> --cyui <CyUI-verrouille> --fetch
```

Les modifications locales du SDK sont préservées : sélectionnez une copie propre
de la révision fixée. Git n’est un repli que si les sources locales sont absentes.
Le nouveau paquet ne contient ni CyToolsCore ni serveur MCP Python. Python stdlib
est conservé uniquement pour les anciens CLI et la migration des tâches.


## Validation de la version 0.2.4

Les essais isolés de production ont vérifié le chat avec un fournisseur HTTP
simulé local, les vrais outils IDE, le CLI et un client MCP externe. Lecture,
modification autorisée, refus de droits/révision, sauvegarde explicite,
annulation/restauration et lecture Unicode complète par morceaux ont réussi.
Les comptes fournisseurs réels et les dialogues interactifs de pièces jointes
restent à valider. Voir les [notes de version](../VERSION.fr.md).
