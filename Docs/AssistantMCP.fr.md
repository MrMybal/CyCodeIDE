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

Le MCP local utilise **Free** par défaut. Dans le panneau MCP, activez/désactivez
le serveur HTTP local et choisissez son profil d’accès. En Free, le client se
connecte avec l’URL `/mcp` affichée, sans jeton ni confirmation supplémentaire
par opération. Toutes les opérations enregistrées sont disponibles ; les schémas
d’arguments, révisions, limites du projet, sauvegardes explicites et règles métier
existantes restent applicables.

| Profil | Jeton HTTP | Opérations |
| --- | --- | --- |
| Free (défaut) | Non | Toutes les opérations enregistrées |
| Local contrôlé | Non | Autorisations accordées par l’hôte |
| Jeton seul | Obligatoire | Toutes les opérations enregistrées |
| Protégé | Obligatoire | Autorisations accordées par l’hôte |

Dans les profils contrôlés, **Autoriser l’édition, les compilations et les commandes
de plugins** accorde les droits existants du Core. **Autoriser la modification des
préférences de l’éditeur** accorde séparément `settings.write`. Les droits sont
contrôlés à chaque appel. Changer de profil ou activer/désactiver HTTP redémarre
ses points d’accès et déconnecte le chat ; reconnectez-le explicitement. Le moteur
courant et les documents ouverts sont conservés.

Les configurations sans choix explicite migrent vers Free. Les profils nommés et
les anciens choix explicites `requireToken` / `controlOperations` sont conservés.
Le choix canonique est enregistré dans `automation.localAccessProfile` ; `enabled`,
`allowEditing` et `allowSettings` restent indépendants. Les droits natifs fichiers
et terminal du fournisseur restent distincts de la politique MCP de l’application.

## CLI et MCP externe

Le CLI du Core et ses anciennes commandes sont conservés. L’exécutable IDE accepte
aussi `--sdk` avant toute création de fenêtre SDL. Pour piloter une fenêtre ouverte,
utilisez **Copier l’adresse MCP** dans le panneau MCP. En Free, l’URL seule suffit :

```text
CyCodeIDE.exe --sdk --help
cycode_cli.exe --sdk --connect <URL-MCP> list-operations
cycode_cli.exe --sdk --connect <URL-MCP> describe-operation ide_state
cycode_cli.exe --sdk --connect <URL-MCP> invoke ide_state --params-file requete.json
cycode_cli.exe --sdk --connect <URL-MCP> invoke ide_open_document --params-file ouvrir.json
```

`requete.json` peut contenir `{ "offset": 0, "limit": 50 }` ; `ouvrir.json` peut
contenir `{ "path": "src/main.cpp", "line": 0, "character": 0 }`. Les positions
sont en UTF-16, indexées à partir de zéro. stdout reçoit le JSON, stderr les erreurs ;
un échec renvoie un code de sortie non nul. Le CLI du Core fonctionne aussi sans
interface sur un projet explicitement choisi. Les opérations `ide_*` exigent une
instance IDE ouverte.

**Copier l’adresse MCP** copie l’URL HTTP ; **Copier la configuration MCP** copie
une configuration HTTP directe, avec en-tête Authorization uniquement pour les
profils à jeton. **Copier la configuration CLI** conserve la connexion stdio native.
Le CLI conserve `--bridge-config <descripteur-prive.json>`. Pour les profils à
jeton, une connexion CLI par URL lit `CYMCP_TOKEN` dans l’environnement du processus.
Le descripteur privé sélectionne la vraie fenêtre. Les points d’accès écoutent
uniquement sur `127.0.0.1` ; descripteurs et éventuels jetons restent privés. Fermer
ou reconfigurer la fenêtre invalide l’accès ; récupérez une nouvelle configuration.
Le profil compact expose cinq outils du SDK : rechercher une opération, charger
son schéma, l’appeler, puis lire/rechercher ses ressources. `--mcp-profile direct-full`
conserve la découverte complète. Les contrats du Core sont préservés ; l’hôte ajoute :

| Opération | Fonction | Permission en profil contrôlé |
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
**0.7.2**, révision `e562c731a146d599e3404718ea8414bb5b96b66d`.
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


## Validation de la version 0.2.5

Quatorze vérifications isolées de production utilisent le vrai IDE : lecture et
modification Free par URL seule, refus des jetons et droits Protégé, quatre profils,
validation des arguments, migration/persistance, CLI et MCP stdio, puis réponses
progressives, annulation et restauration sans rejeu du chat avec fournisseur local
simulé. Les comptes réels et dialogues interactifs de pièces jointes restent non
vérifiés. Voir les [notes de version](../VERSION.fr.md).
