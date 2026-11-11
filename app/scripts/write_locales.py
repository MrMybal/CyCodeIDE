"""Maintain the native UI catalogue. English source keys remain stable IDs."""
import json
from pathlib import Path
translations = {
    "Indexing files...": "Indexation des fichiers...",
    "Updates are unavailable in this development build.": "Les mises à jour sont indisponibles dans cette version de développement.",
    'Run':'Executer',
    'Debugger':'Debogueur',
    'Name':'Nom',
    'Value':'Valeur',
    'Type':'Type',
    'Enabled':'Actif',
    'Line':'Ligne',
    'Reload':'Recharger',
    'Local':'Variables locales',
    'Locals':'Variables locales',
    'Static':'Variables statiques',
    'Registers':'Registres',
    'Debug / Continue':'Deboguer / Continuer',
    'Run without debugging':'Executer sans debogage',
    'Pause program':'Suspendre le programme',
    'Stop debugging':'Arreter le debogage',
    'Step over':'Pas a pas principal',
    'Step into':'Pas a pas detaille',
    'Step out':'Sortir de la fonction',
    'Toggle breakpoint':'Basculer le point d’arret',
    'Breakpoints':'Points d’arret',
    'Condition':'Condition',
    'Hit condition':'Condition de passage',
    'Log message':'Message de journal',
    'Breakpoints changed. Reload before applying.':'Les points d’arret ont change. Rechargez-les avant d’appliquer.',
    'Apply breakpoint changes':'Appliquer les points d’arret',
    'Read-only debugger source':'Source du debogueur en lecture seule',
    'Program terminal':'Terminal du programme',
    'Pause or hit a breakpoint to inspect variables.':'Suspendez le programme ou atteignez un point d’arret pour examiner les variables.',
    'Variables':'Variables',
    'Call stack':'Pile des appels',
    'Watch':'Expressions espionnes',
    'C++ expression':'Expression C++',
    'C++ / C# expression':'Expression C++ / C#',
    'Program / assembly':'Programme / assembly',
    'Evaluate':'Evaluer',
    'Add watch':'Ajouter une expression espionne',
    'Debug console':'Console de debogage',
    'Program input/output is available in Program terminal.':'Les entrees et sorties du programme sont dans Terminal du programme.',
    'Launch configuration':'Configuration de lancement',
    'Executable':'Executable',
    'Working directory':'Dossier de travail',
    'Arguments (one per line)':'Arguments (un par ligne)',
    'Environment (KEY=VALUE)':'Environnement (CLE=VALEUR)',
    'Debug adapter':'Adaptateur de debogage',
    'Build before debugging':'Compiler avant le debogage',
    'Stop on entry':'Arreter au point d’entree',
    'Integrated program terminal':'Terminal integre du programme',
    'Save debug profile':'Enregistrer le profil de debogage',
    'Reload profile':'Recharger le profil',
    'Attach to process ID':'Attacher au processus (ID)',
    'Attach':'Attacher',
    'Select an executable in the debug profile.':'Selectionnez un executable dans la configuration de lancement.',
    'idle':'Inactif',
    'starting':'Demarrage',
    'running':'En execution',
    'stopped':'Suspendu',
    'exited':'Termine',
    'failed':'Echec',
    'Terminal':'Terminal',
    'New terminal':'Nouveau terminal',
    'New terminal (Find action)':'Nouveau terminal (Recherche d’action)',
    'Close terminal and its processes':'Fermer le terminal et ses processus',
    'Clear scrollback':'Effacer l’historique',
    'Open a terminal to run commands in this workspace. Alt+F12 focuses the terminal.':'Ouvrez un terminal pour executer des commandes dans ce projet. Alt+F12 active le terminal.',
    'Refactor':'Refactoriser',
    'Refactoring':'Refactorisation',
    'Rename symbol':'Renommer le symbole',
    'Format document':'Formater le document',
    'Format selection':'Formater la selection',
    'Undo refactoring':'Annuler la refactorisation',
    'Apply refactoring':'Appliquer la refactorisation',
    'Go to type...':'Aller a un type...',
    'Go to symbol...':'Aller a un symbole...',
    'File structure':'Structure du fichier',
    'Types':'Types',
    'Symbols':'Symboles',
    'Members':'Membres',
    'Searching symbols...':'Recherche des symboles...',
    'Results limited. Refine the symbol name.':'Resultats limites. Precisez le nom du symbole.',
    'Type a name; > actions, @ members, # symbols...':'Saisissez un nom ; > actions, @ membres, # symboles...',
    'Read-only SDK source':'Source SDK en lecture seule',
    'Preview changes':'Apercu des modifications',
    'Apply changes':'Appliquer les modifications',
    'No changes required':'Aucune modification necessaire',
    'Refactoring undone':'Refactorisation annulee',
    'Changes applied to live buffers':'Modifications appliquees aux buffers',
    'Changes are in live buffers. Save all to write them to disk.':'Les modifications sont dans les buffers. Enregistrez-les pour les ecrire sur disque.',
    'Rename a C++ symbol to preview changes across its references.':'Renommez un symbole C++ pour examiner les modifications de ses references.',
    'Before':'Avant',
    'After':'Apres',
    'Changed line':'Ligne modifiee',
    'files':'fichiers',
    'edits':'modifications',
    'Renames C++ references. Blueprint assets and serialized Unreal names are not modified.':'Renomme les references C++. Les assets Blueprint et les noms Unreal serialises ne sont pas modifies.',
    'Editor': 'Editeur',
    'Keyboard shortcuts':'Raccourcis clavier',
    'Keymap profile':'Profil de raccourcis',
    'Command':'Commande',
    'Shortcut':'Raccourci',
    'Context':'Contexte',
    'Global':'Global',
    'Document':'Document',
    'Filter commands...':'Filtrer les commandes...',
    'Choose a command to change its shortcuts. Separate alternatives with ; and chord steps with a comma. An empty value disables the shortcut.':'Selectionnez une commande pour modifier ses raccourcis. Separez les variantes avec ; et les etapes avec une virgule. Une valeur vide desactive le raccourci.',
    'Apply shortcut':'Appliquer le raccourci',
    'Reset command':'Reinitialiser la commande',
    'Reset profile':'Reinitialiser le profil',
    'Search Everywhere':'Rechercher partout',
    'Find action...':'Rechercher une action...',
    'Recent files':'Fichiers recents',
    'All':'Tout',
    'Files':'Fichiers',
    'Actions':'Actions',
    'Type a name, or > for actions...':'Saisissez un nom, ou > pour les actions...',
    'No matching results':'Aucun resultat',
    'Go to line...':'Aller a la ligne...',
    'Go':'Aller',
    'Close file':'Fermer le fichier',
    'Close all files':'Fermer tous les fichiers',
    'Reopen closed file':'Rouvrir le fichier ferme',
    'Cut':'Couper',
    'Copy':'Copier',
    'Paste':'Coller',
    'Select all':'Tout selectionner',
    'Duplicate selection or line':'Dupliquer la selection ou la ligne',
    'Delete line':'Supprimer la ligne',
    'Move line up':'Monter la ligne',
    'Move line down':'Descendre la ligne',
    'Toggle line comment':'Commenter ou decommenter les lignes',
    'Comment lines':'Commenter les lignes',
    'Uncomment lines':'Decommenter les lignes',
    'Indent lines':'Indenter les lignes',
    'Unindent lines':'Desindenter les lignes',
    'Extend selection':'Etendre la selection',
    'Shrink selection':'Reduire la selection',
    'Add next occurrence':'Ajouter la prochaine occurrence',
    'Select all occurrences':'Selectionner toutes les occurrences',
    'Selection to uppercase':'Selection en majuscules',
    'Selection to lowercase':'Selection en minuscules',
    'Remove trailing whitespace':'Supprimer les espaces en fin de ligne',
    'Replace in file':'Remplacer dans le fichier',
    'Find next':'Occurrence suivante',
    'Navigate back':'Emplacement precedent',
    'Navigate forward':'Emplacement suivant',
    'Next document':'Document suivant',
    'Previous document':'Document precedent',
    'Prepare Unreal code analysis':"Preparer l'analyse Unreal",
    'Toggle word wrap':'Activer le retour a la ligne',
    'Toggle line numbers':'Afficher les numeros de ligne',
    'Toggle whitespace':'Afficher les espaces',
    'Toggle minimap':'Afficher la minimap',
    'Fold region':'Replier le bloc',
    'Unfold region':'Deplier le bloc',
    'Unfold all':'Tout deplier',
    'Refresh files':'Actualiser les fichiers',
    'Refresh version control':'Actualiser le controle de versions',
    'Loading files...':'Chargement des fichiers...', 'No matching suggestions':'Aucune proposition correspondante',
    'Preparing Unreal code analysis...':"Preparation de l'analyse Unreal...", 'Unreal code analysis ready':'Analyse Unreal prete',
    'Unreal analysis failed. See Build output.':"Echec de l'analyse Unreal. Consultez la sortie de compilation.",
    'Retry Unreal analysis':"Relancer l'analyse Unreal", 'Includes, reflection macros and generated headers are being prepared in the background.':"Les includes, macros de reflection et en-tetes generes sont prepares en arriere-plan.",

    'Go to file   Ctrl+Alt+F':'Ouvrir un fichier   Ctrl+Alt+F',

    'Unreal Engine':'Unreal Engine', 'No Unreal project':'Aucun projet Unreal',
    'Open a .uproject or its .sln solution.':'Ouvrez un .uproject ou sa solution .sln.',
    'Engine directory':'Dossier du moteur', 'Platform':'Plateforme', 'Targets':'Cibles', 'Modules':'Modules', 'Plugins':'Plugins',
    'Refresh Unreal project':'Actualiser le projet Unreal', 'Generate project files':'Generer les fichiers de projet',
    'Generate code index':"Generer l'index du code", 'Select the matching Unreal engine directory.':'Selectionnez le dossier de la bonne version du moteur Unreal.',
    'This project uses UnrealBuildTool.':'Ce projet utilise UnrealBuildTool.',
    'Generate the code index for Unreal include paths, macros and symbol navigation. The engine SDK and generated headers are required.':"Generez l'index du code pour les includes Unreal, macros et symboles. Le SDK du moteur et les en-tetes generes sont requis.",

    'Graphite':'Graphite', 'Open solution...':'Ouvrir une solution...', 'New window':'Nouvelle fenetre',
    'Open solution in new window...':'Ouvrir une solution dans une nouvelle fenetre...',
    'Move to new window':'Deplacer dans une nouvelle fenetre', 'Return to editor':"Reancrer dans l'editeur",
    'Solution':'Solution', 'Workspace files':'Fichiers du projet', 'Missing':'Absent', 'External':'Externe',
    'Missing project files':'Fichiers de projet absents', 'Open project file':'Ouvrir le fichier de projet',
    'Open in new window':'Ouvrir dans une nouvelle fenetre',
    'Generated Unreal project files may be absent. Available source files remain accessible below.':'Les fichiers de projet Unreal generes peuvent etre absents. Les sources presentes restent accessibles ci-dessous.',

    'CMake':'CMake', 'CMake settings':'Reglages CMake', 'Detect toolchains':'Detecter les toolchains',
    'Refresh CMake project':'Actualiser le projet CMake', 'Toolchain':'Toolchain', 'Generator':'Generateur',
    'Configuration':'Configuration', 'Target':'Cible', 'All targets':'Toutes les cibles', 'Automatic':'Automatique',
    'Compilation database ready':'Base de compilation prete', 'Configure for code analysis':"Configurer pour l'analyse du code",
    'translation units':'unites de compilation', 'Settings apply on configure.':"Les reglages s'appliquent a la configuration.",
    'Navigate':'Navigation', 'Go to definition':'Aller a la definition', 'Find references':'Rechercher les references',
    'References':'References', 'Symbol navigation':'Navigation dans le code',
    'F12: definition. Shift+F12: references.':'F12 : definition. Maj+F12 : references.',
    'No locations found':'Aucun emplacement trouve',
    'Some locations are outside this workspace.':'Certains emplacements sont hors de cet espace de travail.',
    'Open a C/C++ document to navigate symbols.':'Ouvrez un document C/C++ pour naviguer dans le code.',
    'Go to file   Ctrl+P':'Ouvrir un fichier   Ctrl+P',
    'New file...':'Nouveau fichier...', 'Workspace-relative path':"Chemin relatif au projet", 'Create':'Creer',
    'Shared Core ready':'Core partage pret', 'Core starting or unavailable':'Core en demarrage ou indisponible',
    'File':'Fichier', 'Edit':'Edition', 'View':'Affichage', 'Build':'Compilation', 'Project':'Projet',
    'Search':'Recherche', 'Version control':'Gestion de versions', 'Problems':'Problemes', 'Extensions':'Extensions',
    'MCP':'MCP', 'Options':'Options', 'Open workspace...':'Ouvrir un espace de travail...', 'Go to file...':'Ouvrir un fichier...',
    'Save all':'Tout enregistrer', 'Exit':'Quitter', 'Undo':'Annuler', 'Redo':'Retablir', 'Find in file':'Rechercher dans le fichier',
    'Reset layout':'Reinitialiser la disposition', 'Configure':'Configurer', 'Build project':'Compiler le projet',
    'Save (Ctrl+S)':'Enregistrer (Ctrl+S)', 'Build (Ctrl+B)':'Compiler (Ctrl+B)',
    'Search everywhere   Ctrl+P':'Rechercher partout   Ctrl+P', 'Ready':'Pret', 'Working...':'Traitement...',
    'No repository':'Aucun depot', 'Refresh':'Actualiser', 'Filter files...':'Filtrer les fichiers...',
    'No files':'Aucun fichier', 'Open a workspace or refresh the file list.':'Ouvrez un espace de travail ou actualisez la liste.',
    'A workspace for your code.':'Un espace de travail pour votre code.', 'Find':'Rechercher', 'Replace':'Remplacer',
    'Find all':'Tout rechercher', 'Replace all':'Tout remplacer', 'Find in workspace...':"Rechercher dans l'espace de travail...",
    'Case sensitive':'Respecter la casse', 'Regex':'Expression reguliere', 'Search your project':'Rechercher dans le projet',
    'Results include unsaved documents for literal searches.':'La recherche de texte inclut les documents non enregistres.',
    'matches':'resultats', 'Results limited':'Resultats limites', 'Regex excludes modified buffers.':'Les expressions regulieres excluent les documents modifies.',
    'Diff':'Differences', 'Staged diff':'Differences preparees', 'History':'Historique', 'No Git repository':'Aucun depot Git',
    'Open a folder containing a Git repository.':'Ouvrez un dossier contenant un depot Git.', 'Commit message':'Message du commit',
    'Commit staged':'Creer le commit', 'Status':'Etat', 'Actions':'Actions', 'Stage':'Preparer', 'Unstage':'Retirer',
    'CMake source directory':'Dossier source CMake', 'Build directory':'Dossier de compilation', 'Copy output':'Copier la sortie',
    'clangd connected':'clangd connecte', 'Starting clangd...':'Demarrage de clangd...', 'clangd stopped':'clangd arrete',
    'Restart language server':'Redemarrer le serveur de langage', 'No diagnostics':'Aucun diagnostic',
    'C++ diagnostics appear here when clangd is connected.':'Les diagnostics C++ apparaissent ici lorsque clangd est connecte.',
    'Plugin ABI':'ABI des plugins', 'Native bridge ready':'Passerelle native prete',
    'Connect an assistant to inspect this workspace and its live documents.':"Connectez un assistant pour consulter le projet et les documents ouverts.",
    'Enable editing, builds and plugin commands':"Autoriser les modifications, compilations et commandes des plugins",
    'Copy MCP configuration':'Copier la configuration MCP', 'Save MCP configuration':'Enregistrer la configuration MCP',
    'Available services':'Services disponibles',
    'Workspace, live documents, search, Git, CMake and native plugin commands. MCP sessions use CyToolsCore jobs and permissions.':"Projet, documents ouverts, recherche, Git, CMake et plugins natifs. Les sessions MCP utilisent les jobs et permissions CyToolsCore.",
    'Language':'Langue', 'Theme':'Theme', 'Text size':'Taille du texte', 'Save preferences':'Enregistrer les preferences',
    'Save configuration':'Enregistrer la configuration', 'Load configuration':'Charger la configuration', 'Licenses':'Licences',
    'CyCode: GPL-3.0-only. Copyright (c) 2026 Cyberalien. This program comes without any warranty. See LICENSE and COPYRIGHT.':"CyCode : GPL-3.0-only. Copyright (c) 2026 Cyberalien. Ce programme est fourni sans aucune garantie. Voir LICENSE et COPYRIGHT.",
    'CyUI, Dear ImGui, TextEditor, CyToolsCore and nlohmann/json: MIT. SDL3: zlib.':"CyUI, Dear ImGui, TextEditor, CyToolsCore et nlohmann/json : MIT. SDL3 : zlib.",
    'Open third-party notices':'Ouvrir les licences des dependances', 'Type a file name...':'Saisissez un nom de fichier...',
    'Unsaved changes':'Modifications non enregistrees', 'Save':'Enregistrer', 'Discard':'Abandonner', 'Cancel':'Annuler',
    'Close workspace?':"Fermer l'espace de travail ?", 'Some documents have unsaved changes.':'Certains documents ne sont pas enregistres.',
    'Save or close modified documents before switching workspace.':"Enregistrez ou fermez les documents modifies avant de changer de projet.",
    'Wait for current tasks before switching workspace.':'Attendez la fin des taches avant de changer de projet.',
    'Workspace opened: ':"Espace de travail ouvert : ", 'A task is already running.':'Une tache est deja en cours.',
    'Saved':'Enregistre', 'File list limited to 20,000 entries.':'Liste limitee a 20 000 fichiers.',
    'Task failed. See output.':'Echec de la tache. Consultez la sortie.', 'Task completed':'Tache terminee'
}
translations.update({'Initialize Git repository': 'Initialiser le depot Git', 'Changes': 'Modifications', 'Review': 'Apercu', 'Branches': 'Branches', 'Remotes': 'Depots distants', 'Stashes': 'Stashes', 'Conflicts': 'Conflits', 'Commit changes': 'Creer un commit', 'Git branches': 'Branches Git', 'Git history': 'Historique Git', 'Git remotes and sync': 'Depots distants et synchronisation Git', 'Git stashes': 'Stashes Git', 'Git conflicts': 'Conflits Git', 'Show file diff': 'Afficher les differences du fichier', 'Annotate file': 'Annoter le fichier', 'Resolve conflict': 'Resoudre le conflit', 'New branch name': 'Nom de la nouvelle branche', 'Create branch': 'Creer une branche', 'Start revision': 'Revision de depart', 'Branch': 'Branche', 'Upstream': 'Branche suivie', 'Track branch': 'Suivre la branche', 'Switch branch': 'Changer de branche', 'Rename branch': 'Renommer la branche', 'Delete merged branch': 'Supprimer la branche fusionnee', 'Merge revision': 'Fusionner la revision', 'Switching and merging require saved buffers and a clean tracked working tree.': 'Enregistrez les documents et creez un commit ou un stash avant de changer ou de fusionner une branche.', 'Commit': 'Commit', 'Author': 'Auteur', 'Date': 'Date', 'Message': 'Message', 'Remote name': 'Nom du depot distant', 'Remote URL': 'URL du depot distant', 'Add remote': 'Ajouter le depot distant', 'Update remote URL': 'Modifier l URL du depot distant', 'Remove remote': 'Retirer le depot distant', 'Set upstream on push': 'Definir la branche suivie au push', 'Fetch': 'Recuperer', 'Pull fast-forward': 'Mettre a jour sans fusion', 'Push': 'Envoyer', 'Uses configured Git credentials and hooks. Pull refuses divergent branches.': 'Utilise les identifiants et hooks Git configures. La mise a jour refuse les branches divergentes.', 'Stash message': 'Message du stash', 'Include untracked': 'Inclure les fichiers non suivis', 'Save stash': 'Creer un stash', 'Apply stash': 'Appliquer le stash', 'Pop stash': 'Appliquer et retirer le stash', 'Drop selected stash...': 'Supprimer le stash selectionne...', 'Drop stash?': 'Supprimer le stash ?', 'Drop stash': 'Supprimer le stash', 'Permanently remove this saved stash entry?': 'Supprimer definitivement ce stash enregistre ?', 'Choose Resolve conflict from a conflicted file in Changes.': 'Choisissez Resoudre le conflit sur un fichier en conflit dans Modifications.', 'Refresh conflict': 'Actualiser le conflit', 'Use ours in editor': 'Utiliser notre version', 'Use theirs in editor': 'Utiliser leur version', 'Save and mark resolved': 'Enregistrer et marquer resolu', 'Edit the working file in its normal tab. These three source versions are read-only.': 'Modifiez le fichier dans son onglet habituel. Les trois versions ci-dessous sont en lecture seule.', 'Base': 'Base', 'Ours': 'Notre version', 'Theirs': 'Leur version', 'libvterm and CodeLLDB: MIT. LLVM: Apache 2.0 with LLVM exceptions. Embedded Python: PSF.': 'libvterm et CodeLLDB : MIT. LLVM : Apache 2.0 avec exceptions LLVM. Python embarque : PSF.'})
translations.update({'MSBuild': 'MSBuild', 'No MSBuild project': 'Aucun projet MSBuild', 'Open a .sln, .vcxproj or managed project.': 'Ouvrez un .sln, .vcxproj ou projet .NET.', 'MSBuild settings': 'Reglages MSBuild', 'This project uses MSBuild.': 'Ce projet utilise MSBuild.', 'Rebuild': 'Recompiler', 'Clean': 'Nettoyer', 'Restore packages': 'Restaurer les paquets', 'Solution / project': 'Solution / projet', 'Configuration / platform': 'Configuration / plateforme', 'Whole solution': 'Toute la solution', 'Build tool': 'Outil de compilation', 'Packages': 'Paquets', 'Restore before build': 'Restaurer avant compilation', 'Evaluate project': 'Evaluer le projet', 'Prepare code analysis': 'Preparer l analyse du code', 'Mapped configuration': 'Configuration du projet', 'Included in solution build': 'Inclus dans la compilation', 'Yes': 'Oui', 'No': 'Non', 'Use in debug profile': 'Utiliser pour le debogage', 'Rebuild MSBuild project': 'Recompiler le projet MSBuild', 'Clean MSBuild project': 'Nettoyer le projet MSBuild', 'Restore MSBuild packages': 'Restaurer les paquets MSBuild', 'Evaluate MSBuild project': 'Evaluer le projet MSBuild', 'Prepare MSBuild code analysis': 'Preparer l analyse MSBuild', 'Evaluation loads project imports and rules. Code analysis uses evaluated C++ include paths, definitions and language modes.': 'L evaluation charge les imports et regles du projet. L analyse utilise les includes, definitions et modes C++ evalues.'})
translations.update({'Build diagnostics':'Erreurs de compilation', 'Enable':'Activer', 'Disable':'Desactiver', 'Language server is not installed':'Serveur de langage non installe', 'Diagnostics from enabled language servers appear here.':'Les diagnostics des serveurs de langage actifs apparaissent ici.'})
translations.update({'Connected':'Connecte', 'Starting...':'Demarrage...', 'Idle':'En attente', 'Disabled':'Desactive', 'Diagnostics from C++ and C# language servers appear here.':'Les diagnostics des serveurs C++ et C# apparaissent ici.'})
translations.update({'csharp-ls: MIT. Private .NET SDK: included Microsoft terms and component notices.':'csharp-ls : MIT. SDK .NET prive : conditions Microsoft et notices des composants incluses.',
    'Workspace, live documents, language services, search, Git, CMake/MSBuild/Unreal, terminal, debugger and plugins. MCP sessions use CyToolsCore jobs and permissions.':'Espace de travail, documents, services de langage, recherche, Git, CMake/MSBuild/Unreal, terminal, debogueur et extensions. Les sessions MCP utilisent les taches et permissions CyToolsCore.'})
translations.update({'Debug engine':'Moteur de debogage','.NET host':'Hote .NET',
    'Managed debug output appears here. Run without debugging supports terminal input.':'La sortie du debogage .NET est dans la console de debogage. Executer sans debogage permet la saisie dans le terminal.'})
translations.update({
    'New file':'Nouveau fichier', 'Save as...':'Enregistrer sous...',
    'Open files...':'Ouvrir des fichiers...', 'Open files':'Fichiers ouverts',
    'Recent files':'Fichiers recents', 'Untitled':'Sans titre', 'Close file':'Fermer le fichier',
    'Reload from disk':'Recharger depuis le disque', 'Find / replace':'Rechercher / remplacer',
    'Find next':'Occurrence suivante', 'Go to line':'Aller a la ligne', 'Go':'Aller',
    'Switch file / actions':'Changer de fichier / actions', 'Word wrap':'Retour a la ligne automatique',
    'Minimap':'Minicarte', 'Show whitespace':'Afficher les espaces', 'Syntax':'Coloration',
    'Automatic':'Automatique', 'Plain text':'Texte brut', 'Code editor':'Editeur de code', 'Lines':'Lignes',
    'Filter files...':'Filtrer les fichiers...', 'Open or drop code files to begin.':'Ouvrez ou deposez des fichiers de code pour commencer.',
    'Save changes before continuing?':'Enregistrer les modifications avant de continuer ?',
    'Replace existing file?':'Remplacer le fichier existant ?', 'Type a file name or action...':'Saisissez un nom de fichier ou une action...',
    'Close a document before opening more files.':'Fermez un document avant d ouvrir plus de fichiers.',
    'This file is already open in another tab.':'Ce fichier est deja ouvert dans un autre onglet.',
    'File changed on disk. Reload it or save a copy.':'Le fichier a change sur le disque. Rechargez-le ou enregistrez une copie.',
    'Read-only':'Lecture seule'
})
translations.update({
    'Project tasks':'Taches du projet', 'Project system':'Systeme de projet', 'Project directory':'Dossier du projet',
    'Run project tests':'Executer les tests du projet', 'Install npm dependencies':'Installer les dependances npm',
    'Run npm script':'Executer le script npm', 'Cancel task':'Annuler la tache', 'Task cancelled':'Tache annulee',
    'Cargo package':'Paquet Cargo', 'Binary target':'Cible executable', 'Detected targets':'Cibles detectees',
    'Select a binary target':'Choisir une cible executable', 'Offline Cargo mode':'Mode Cargo hors ligne',
    'npm script':'Script npm', 'Java source directory':'Dossier des sources Java', 'Java main class':'Classe principale Java',
    'Select a main class':'Choisir une classe principale',
    'No Cargo, npm or standalone Java project':'Aucun projet Cargo, npm ou Java autonome',
    'Open Cargo.toml, package.json, a Java source file or its folder.':'Ouvrez Cargo.toml, package.json, un fichier Java ou son dossier.',
    'Standalone Java: compilation and Run. Maven, Gradle and Java debugging are not connected.':'Java autonome : compilation et execution. Maven, Gradle et le debogage Java ne sont pas raccordes.',
    'Project metadata changed; configure or build again.':'Les metadonnees du projet ont change ; configurez ou compilez de nouveau.',
    'Debugging is available here for Cargo targets.':'Le debogage est disponible ici pour les cibles Cargo.'
})
translations.update({
    'Code information':'Informations sur le code', 'Quick documentation':'Documentation rapide',
    'Parameter information':'Informations sur les parametres', 'Copy information':'Copier les informations',
    'Request documentation or parameter information at the cursor.':'Demandez la documentation ou les parametres a la position du curseur.',
    'No information at this position.':'Aucune information a cette position.', 'Overload':'Surcharge',
    'Active parameter':'Parametre actif', 'Information truncated':'Informations tronquees'
})
translations.update({
    'Completion with imports requires a single cursor.':'La completion avec imports necessite un seul curseur.',
    'Completion context changed. Request suggestions again.':'Le contexte a change. Relancez les suggestions.'
})
translations.update({'Snippet parameter':'Parametre du snippet', 'Snippet choices':'Choix du snippet',
    'Tab: next | Shift+Tab: previous | Esc: finish':'Tab : suivant | Maj+Tab : precedent | Echap : terminer'})
translations.update({
    'Quick fixes and context actions':'Corrections rapides et actions contextuelles',
    'Code actions':'Actions sur le code', 'Refresh code actions':'Actualiser les actions sur le code',
    'All context actions':'Toutes les actions contextuelles', 'Quick fixes':'Corrections rapides',
    'Refactorings':'Refactorisations', 'Organize imports':'Organiser les imports',
    'Loading code actions':'Chargement des actions', 'Action':'Action', 'Kind':'Catégorie',
    'Apply correction':'Appliquer la correction',
    'Code action context changed. Refresh the list.':'Le contexte a changé. Actualisez la liste des actions.',
    'Request fixes or refactorings at the cursor or selection.':'Demandez les corrections ou refactorisations au curseur ou sur la sélection.',
    'No code actions at this position.':'Aucune action à cette position.',
    'Code action list truncated':'Liste des actions tronquée'
})
translations.update({'Wrap lines':'Retour à la ligne', 'Source':'Source', 'Position':'Position',
    'Message':'Message', 'Follow':'Suivre', 'Find All':'Tout rechercher', 'Replace All':'Tout remplacer'})
translations.update({'Open image...': 'Ouvrir une image...', 'Fit image': 'Ajuster l’image', 'Zoom out': 'Dézoomer', 'Zoom in': 'Zoomer', 'Reload image': 'Recharger l’image', 'Pixelated': 'Pixels nets', 'Image preview': 'Aperçu de l’image', 'Cannot load image': 'Impossible de charger l’image', 'Loading image...': 'Chargement de l’image...', 'Decoding in the background.': 'Décodage en arrière-plan.', 'GIF preview: first frame': 'Aperçu GIF : première image', 'HDR preview converted to 8-bit display': 'Aperçu HDR converti pour un affichage 8 bits', 'Wheel: zoom | Middle mouse or Space+drag: pan | Double-click: fit': 'Molette : zoom | Bouton central ou Espace+glisser : déplacement | Double-clic : ajuster', 'Cannot read image file': 'Impossible de lire le fichier image', 'Image file is empty or exceeds the 64 MiB input limit': 'Le fichier image est vide ou dépasse la limite de 64 Mio', 'Image dimensions exceed the preview limit (32768 per side, 64 megapixels)': 'Les dimensions dépassent la limite de l’aperçu (32768 par côté, 64 mégapixels)', 'Animated WebP preview is not supported yet': 'L’aperçu WebP animé n’est pas encore pris en charge', "Image dimensions exceed this GPU's texture limit": 'Les dimensions dépassent la limite de texture de cette carte graphique', 'Image preview texture budget reached; close another image and reload': 'La limite mémoire des aperçus est atteinte ; fermez une autre image puis rechargez', 'The GPU could not allocate the image preview': 'La carte graphique n’a pas pu créer l’aperçu', 'Too many image previews queued': 'Trop d’aperçus d’images en attente', 'Unsupported or invalid image (PNG, JPEG, WebP, GIF, BMP, TGA, PSD, HDR, PIC, PGM/PPM are supported)': 'Image invalide ou non prise en charge (formats : PNG, JPEG, WebP, GIF, BMP, TGA, PSD, HDR, PIC, PGM/PPM)'})
translations.update({'Select an image file':'Sélectionnez un fichier image', 'The selected image path now points to another file':'Le chemin sélectionné pointe maintenant vers un autre fichier'})
translations.update({
    'Refresh extensions':'Actualiser les extensions',
    'Project / repository folder':'Dossier du projet / dépôt',
    'Configured provider is unavailable':'Le fournisseur configuré est indisponible',
    'Waiting for version control context.':'Actualisation du contexte de contrôle de versions en cours.',
    'Enable for this project':'Activer pour ce projet',
    'Disable for this project':'Désactiver pour ce projet',
    'Extensions are disabled for this session (--disable-plugins).':'Les extensions sont désactivées pour cette session (--disable-plugins).',
    'Provider mapping root':'Racine du fournisseur',
    'Version control provider':'Fournisseur de contrôle de versions',
    'Configured provider is unavailable. Enable its extension or select another provider.':'Le fournisseur configuré est indisponible. Activez son extension ou choisissez un autre fournisseur.',
    'No custom actions are advertised by this provider.':'Ce fournisseur ne propose aucune action personnalisée.',
    'Unreal project overview':'Vue d’ensemble du projet Unreal',
    'Unreal: inspect':'Unreal : inspecter le projet',
    'Unreal: generate':'Unreal : générer les fichiers de projet',
    'Unreal: prepare':'Unreal : préparer l’analyse',
    'Unreal: database':'Unreal : générer la base de compilation',
    'Unreal: build':'Unreal : compiler',
    'File disappeared from disk; the open buffer is preserved.':'Le fichier a disparu du disque ; le document ouvert est conservé.',
    'File index is incomplete; refine the project scope or refresh.':'L’index des fichiers est incomplet ; affinez le périmètre du projet ou actualisez.'
})
translations.update({
    'Recovery':'Récupération',
    'Unsaved documents can be recovered.':'Des documents non enregistrés peuvent être récupérés.',
    'Checkpoint now':'Sauvegarder les buffers maintenant',
    'Automatic checkpoints: every second. Project files are saved only on request.':'Sauvegarde des buffers toutes les secondes. Les fichiers du projet sont enregistrés sur demande.',
    'Recovery checkpoints could not be saved. Open Recovery for details.':'Impossible de sauvegarder les buffers de récupération. Consultez le panneau Récupération.',
    'No recovery checkpoints in this workspace.':'Aucune sauvegarde de récupération dans cet espace de travail.',
    'Recovery list truncated; resolve checkpoints and refresh.':'Liste de récupération tronquée ; traitez les sauvegardes puis actualisez.',
    'Available for recovery':'Disponible pour récupération', 'In use':'En cours d’utilisation',
    'The original file is missing. Restore recovers its buffer; Save recreates the file.':'Le fichier d’origine est absent. Restaurez son buffer, puis enregistrez-le pour recréer le fichier.',
    'The file changed on disk. Restore keeps a save conflict for review.':'Le fichier a changé sur disque. La restauration conserve un conflit à examiner avant enregistrement.',
    'Save or close the modified target, then preview again.':'Enregistrez ou fermez le document modifié, puis actualisez l’aperçu.',
    'Restore buffer':'Restaurer le buffer', 'Preview again':'Actualiser l’aperçu',
    'Discard checkpoint...':'Supprimer la sauvegarde...', 'Discard checkpoint':'Supprimer la sauvegarde',
    'Discard recovery checkpoint?':'Supprimer cette sauvegarde de récupération ?',
    'This removes only the selected backup. The project file stays unchanged.':'Seule la sauvegarde sélectionnée sera supprimée. Le fichier du projet reste inchangé.',
    'Preview truncated; restoration includes the complete buffer.':'Aperçu tronqué ; la restauration récupère le buffer complet.',
    'Select an inactive checkpoint to preview and restore it.':'Sélectionnez une sauvegarde inactive pour l’examiner et la restaurer.',
    'Recovered in memory; the source checkpoint was retained.':'Document récupéré en mémoire ; la sauvegarde d’origine a été conservée.'
})
translations.update({
    'Files':'Fichiers', 'New folder...':'Nouveau dossier...', 'Rename or move...':'Renommer ou déplacer...',
    'Move to project trash...':'Déplacer dans la corbeille du projet...', 'Undo file operation':'Annuler l’opération sur les fichiers',
    'Show in Files':'Afficher dans Fichiers', 'Project trash':'Corbeille du projet', 'Restore...':'Restaurer...',
    'File operation':'Opération sur les fichiers', 'File operation completed':'Opération sur les fichiers terminée',
    'File history was truncated; open documents are reconciled by identity.':'Historique des fichiers tronqué ; les documents ouverts sont synchronisés par leur identité.',
    'Folder listing truncated. Narrow the folder scope.':'Liste du dossier tronquée. Ouvrez un sous-dossier.',
    'Deleted files stay here until explicitly restored. No automatic expiry.':'Les fichiers supprimés restent ici jusqu’à leur restauration. Aucune expiration automatique.',
    'Trash list truncated. Restore entries and refresh.':'Liste de la corbeille tronquée. Restaurez des éléments puis actualisez.',
    'Physical paths only: includes, project manifests and debugger breakpoints are not rewritten.':'Seuls les chemins physiques sont modifiés : les includes, fichiers de projet et points d’arrêt ne sont pas réécrits.',
    'Destination':'Destination', 'Original path by default':'Chemin d’origine par défaut',
    'Preview operation':'Prévisualiser l’opération', 'Apply file operation':'Appliquer l’opération', 'Entries':'éléments',
    'Save or reconcile affected documents, then preview again.':'Enregistrez ou résolvez les conflits des documents concernés, puis relancez l’aperçu.'
})
translations.update({
    'New project...':'Nouveau projet...', 'Project template':'Modèle de projet', 'Project name':'Nom du projet',
    'Project templates changed; preview again.':'Les modèles de projet ont changé ; relancez l’aperçu.',
    'Unreal Engine C++ game':'Jeu C++ Unreal Engine', 'Engine association':'Association du moteur',
    'Installed engine version or source-build association GUID':'Version du moteur installé ou GUID associé à une compilation du moteur',
    'Close':'Fermer',
    'Parent directory':'Dossier parent', 'Target framework':'Framework cible', 'Required tools':'Outils nécessaires',
    'Preview project':'Prévisualiser le projet', 'Create and open':'Créer et ouvrir', 'Project created':'Projet créé',
    'Open created project':'Ouvrir le projet créé', 'Open in new window':'Ouvrir dans une nouvelle fenêtre',
    'Preview the generated files before creating a new project directory.':'Examinez les fichiers générés avant de créer un nouveau dossier de projet.',
    'C++ console (CMake)':'Console C++ (CMake)', 'C console (CMake)':'Console C (CMake)',
    'Rust console (Cargo)':'Console Rust (Cargo)', 'C# console (.NET solution)':'Console C# (solution .NET)',
    'Java console (standalone)':'Console Java (autonome)', 'Web (HTML / CSS / JavaScript)':'Web (HTML / CSS / JavaScript)'
})
translations.update({'Hunks':'Blocs de modifications','Review hunks':'Examiner les blocs','Staged hunks':'Blocs indexés',
    'Stage selected hunks':'Indexer les blocs sélectionnés','Unstage selected hunks':'Désindexer les blocs sélectionnés',
    'Use whole-file staging for binary, added, deleted or mode-changing files':'Indexez le fichier entier pour les fichiers binaires, ajoutés, supprimés ou avec un changement de mode'})
translations.update({'Tests':'Tests','Test':'Test','Discover CTest tests':'Découvrir les tests CTest','Run selected tests':'Exécuter les tests sélectionnés',
    'Filter tests':'Filtrer les tests','Select all':'Tout sélectionner','Clear selection':'Effacer la sélection','Labels':'Étiquettes',
    'Not run':'Non exécuté','Passed':'Réussi','Failed':'Échoué','Timed out':'Délai dépassé','Cancelled':'Annulé','Test run cancelled':'Exécution des tests annulée',
    'Results include unsaved documents.':'Les résultats incluent les documents non enregistrés.'})
translations.update({'Configure language server...':'Configurer le serveur de langage...', 'Language server settings':'Paramètres du serveur de langage',
    'Executable arguments (JSON array)':'Arguments de l’exécutable (tableau JSON)','Server settings (JSON object)':'Paramètres du serveur (objet JSON)',
    'Apply and restart':'Appliquer et redémarrer','Server log':'Journal du serveur','Cancel tests':'Annuler les tests','Output limit':'Limite de sortie atteinte'})
translations.update({'Activation notifications supported':'Notifications d’activation prises en charge'})
translations.update({'Cancel build':'Annuler la compilation', 'Cancelling build...':'Arrêt de la compilation...'})
translations.update({'Configure preset':'Préréglage de configuration','Build preset':'Préréglage de compilation',
    'Manual settings':'Réglages manuels','Default build':'Compilation par défaut'})
translations.update({'Build time limit (seconds)':'Durée limite de compilation (secondes)',
    'Applies to CMake, MSBuild and Unreal build processes. Cancel remains available.':'S’applique aux compilations CMake, MSBuild et Unreal. L’annulation reste disponible.'})
translations.update({'Event subscription':'Abonnement aux événements',
    'Disable and enable the plugin to restart its event subscription.':'Désactivez puis réactivez le plugin pour redémarrer son abonnement aux événements.',
    'Last workspace event':'Dernier événement du projet','Observed events':'Événements observés'})
translations.update({'Use installed defaults':'Utiliser les paramètres installés',
    'Empty arguments use the installed server. No shell is used.':'Des arguments vides utilisent le serveur installé. Aucune commande shell n’est exécutée.',
    'Remember for this project':'Mémoriser pour ce projet',
    'Saved in local CyCode preferences, outside project configuration.':'Enregistré dans les préférences locales de CyCode, hors de la configuration du projet.',
    'Reload saved settings':'Recharger les paramètres enregistrés','Forget saved settings':'Oublier les paramètres enregistrés',
    'Forgetting keeps the current server running; defaults return next session.':'Le serveur actuel reste actif ; les paramètres par défaut reviennent à la prochaine session.'})
translations.update({'Additional diagnostics are available in Problems.':'D’autres diagnostics sont disponibles dans Problèmes.'})
path = Path(__file__).resolve().parents[1] / 'locales/ui.json'
translations.update({'Adapter provider':'Fournisseur du débogueur','Built-in C++ / .NET':'Débogueur C++ / .NET intégré',
    'Adapter options (JSON)':'Options de l’adaptateur (JSON)',
    'Selected debug adapter is unavailable. Enable its plugin or choose another adapter.':'L’adaptateur choisi est indisponible. Activez son plugin ou choisissez-en un autre.',
    'Adapter-specific launch/attach parameters. Use the common fields for program, arguments, environment and working directory.':'Paramètres de lancement ou d’attachement propres à l’adaptateur. Utilisez les champs communs pour le programme, ses arguments, son environnement et son dossier de travail.'})
translations.update({'Subversion (SVN)':'Subversion (SVN)',
    'SVN working-copy diff':'Différences de la copie SVN', 'SVN history':'Historique SVN',
    'SVN file annotations':'Annotations du fichier SVN','Add file to SVN':'Ajouter le fichier à SVN',
    'Revert SVN file':'Rétablir le fichier SVN','Commit SVN changes':'Envoyer les modifications SVN',
    'Update SVN working copy':'Mettre à jour la copie SVN',
    'Type REVERT to discard local changes':'Saisir REVERT pour abandonner les modifications locales'})
translations.update({'Perforce (P4)':'Perforce (P4)', 'Perforce changelist':'Changelist Perforce',
    'Perforce project diff':'Différences du projet Perforce','Perforce submitted history':'Historique des soumissions Perforce',
    'Perforce file annotations':'Annotations du fichier Perforce','Check out Perforce file':'Ouvrir le fichier Perforce en modification',
    'Add file to Perforce':'Ajouter le fichier à Perforce',
    'Move file to Perforce changelist':'Changer la changelist du fichier Perforce',
    'Revert Perforce file':'Rétablir le fichier Perforce','Submit Perforce file':'Soumettre le fichier Perforce',
    'Submit description':'Description de la soumission','Sync clean Perforce project':'Synchroniser le projet Perforce sans modifications',
    'Lock Perforce file':'Verrouiller le fichier Perforce','Unlock Perforce file':'Déverrouiller le fichier Perforce'})
translations.update({'Mercurial (Hg)':'Mercurial (Hg)',
    'Mercurial working-copy diff':'Différences de la copie Mercurial','Mercurial history':'Historique Mercurial',
    'Mercurial file annotations':'Annotations du fichier Mercurial','Add file to Mercurial':'Ajouter le fichier à Mercurial',
    'Stop tracking Mercurial file':'Ne plus suivre le fichier Mercurial','Revert Mercurial file':'Rétablir le fichier Mercurial',
    'Commit Mercurial changes':'Enregistrer les modifications Mercurial',
    'Update clean Mercurial working copy':'Mettre à jour la copie Mercurial sans modifications',
    'Mercurial revision':'Révision Mercurial','Mercurial name':'Nom Mercurial',
    'Create Mercurial bookmark':'Créer un signet Mercurial','Create Mercurial named branch':'Créer une branche nommée Mercurial',
    'Configured Mercurial remote':'Alias distant Mercurial configuré',
    'Pull Mercurial changes without updating files':'Récupérer les modifications Mercurial sans changer les fichiers',
    'Push Mercurial changes':'Envoyer les modifications Mercurial'})
translations.update({'Diversion':'Diversion',
    'Diversion working-copy diff':'Différences de la copie Diversion', 'Diversion history':'Historique Diversion',
    'Diversion file annotations':'Annotations du fichier Diversion',
    'Commit Diversion changes (auto-sync)':'Enregistrer les modifications Diversion (synchronisation automatique)',
    'Reset Diversion file':'Rétablir le fichier Diversion',
    'Create Diversion branch without switching':'Créer une branche Diversion sans basculer',
    'Diversion branch name':'Nom de la branche Diversion', 'Diversion reference':'Référence Diversion',
    'Switch clean Diversion workspace':'Changer de branche Diversion sans modifications',
    'Update clean Diversion workspace':'Mettre à jour le projet Diversion sans modifications',
    'Lock Diversion file':'Verrouiller le fichier Diversion', 'Unlock Diversion file':'Déverrouiller le fichier Diversion'})
translations.update({'Scopes / Registers':'Portées / Registres','Register scopes appear when provided by the selected debug adapter.':'Les registres apparaissent lorsque le débogueur les fournit.',
    'Select a scope':'Sélectionner une portée','All variables':'Toutes les variables','Read scope page':'Lire la page de variables',
    'Previous page':'Page précédente','Next page':'Page suivante','Memory':'Mémoire','Memory reference':'Référence mémoire',
    'This debug adapter does not advertise memory reads.':'Ce débogueur ne propose pas la lecture mémoire.',
    'Use evaluated memory reference':'Utiliser la référence mémoire évaluée','Byte offset':'Décalage en octets',
    'Bytes to read':'Octets à lire','Read memory':'Lire la mémoire','Address':'Adresse','Unreadable bytes':'Octets illisibles',
    'Disassembly':'Désassemblage','This debug adapter does not advertise disassembly.':'Ce débogueur ne propose pas le désassemblage.',
    'Instruction reference':'Référence d’instruction','Use current instruction':'Utiliser l’instruction actuelle',
    'Instruction offset':'Décalage en instructions','Read instructions':'Lire les instructions','Bytes':'Octets','Instruction':'Instruction','Symbol':'Symbole',
    'Function / Exception breakpoints':'Points d’arrêt de fonction / exception','Function breakpoints':'Points d’arrêt de fonction',
    'This debug adapter does not advertise function breakpoints.':'Ce débogueur ne propose pas les points d’arrêt de fonction.',
    'Function name':'Nom de la fonction','Add function breakpoint':'Ajouter un point d’arrêt de fonction',
    'Verified':'Vérifié','Unverified':'Non vérifié','Exception breakpoints':'Points d’arrêt d’exception',
    'Start a debug session to discover its exception filters.':'Lancez une session de débogage pour découvrir ses filtres d’exception.',
    'Use adapter exception defaults':'Utiliser les filtres d’exception par défaut du débogueur',
    'Inspect stopped exception':'Examiner l’exception à l’arrêt'})
translations.update({'No Cargo, npm, Java, Maven or Gradle project':'Aucun projet Cargo, npm, Java, Maven ou Gradle',
    'Open a project manifest or its folder.':'Ouvrez le manifeste d’un projet ou son dossier.',
    'Gradle module (empty selects all)':'Module Gradle (vide : tous les modules)','Build task':'Tâche de compilation',
    'Offline dependency resolution':'Résolution des dépendances hors ligne','Task timeout (seconds)':'Délai des tâches (secondes)',
    'Run build task':'Exécuter la tâche de compilation',
    'Maven/Gradle run project-defined builds and tests. Select a main class to run with the resolved runtime classpath. Built-in Java debugging is not connected.':'Maven/Gradle utilisent les règles de compilation et de test du projet. Sélectionnez une classe principale pour l’exécuter avec ses dépendances. Le débogueur Java intégré n’est pas connecté.',
    'Standalone Java: compilation and Run. Select Maven or Gradle for build-tool projects.':'Java autonome : compilation et exécution. Sélectionnez Maven ou Gradle pour les projets correspondants.'})
translations.update({'Restore last workspace session':'Restaurer la dernière session du projet'})
translations.update({'Use shader tasks in this project':'Utiliser les tâches de shader dans ce projet',
    'Shader source (workspace path)':'Source du shader (chemin dans le projet)',
    'Detected shader sources':'Sources de shader détectées','Select a shader source':'Sélectionner un shader',
    'Shader language':'Langage du shader','Shader stage':'Étape du shader','Shader target':'Cible du shader',
    'Shader entry point':'Point d’entrée du shader','HLSL shader model':'Modèle de shader HLSL',
    'Shader macros (one NAME=value per line)':'Macros du shader (un NOM=valeur par ligne)',
    'Shader include directories (workspace paths)':'Dossiers d’inclusion du shader (chemins dans le projet)'})
translations.update({'Plugin packages':'Paquets d’extension',
    'Some stored packages could not be verified and were left untouched.':'Certains paquets stockés n’ont pas pu être vérifiés et ont été conservés sans modification.',
    'Signed local packages. Changes apply to the next startup.':'Paquets locaux signés. Les changements s’appliquent au prochain démarrage.',
    'Choose package folder':'Choisir le dossier du paquet','Refresh package catalogue':'Actualiser les paquets',
    'Package folder':'Dossier du paquet','Inspect package':'Inspecter le paquet',
    'The selected packages differ from this host. Restart CyCode after resolving any errors.':'La sélection diffère de cette instance. Redémarrez CyCode après résolution des erreurs.',
    'Signature and payload verified':'Signature et fichiers vérifiés','Files / bytes':'Fichiers / octets',
    'Publisher fingerprint (SHA-512)':'Empreinte de l’éditeur (SHA-512)','Copy publisher fingerprint':'Copier l’empreinte de l’éditeur',
    'Requested permissions':'Permissions demandées','No exported host permissions requested.':'Aucune permission des services hôtes demandée.',
    'This package is incompatible with the current host/platform.':'Ce paquet est incompatible avec cette version ou cette plateforme.',
    'Native plugins run with your account privileges. Verify the publisher fingerprint before trusting it.':'Les extensions natives disposent des droits de votre compte. Vérifiez l’empreinte de l’éditeur avant de lui faire confiance.',
    'Publisher label (local)':'Nom local de l’éditeur','I trust this publisher to run native code':'J’autorise cet éditeur à exécuter du code natif',
    'Trust publisher':'Faire confiance à cet éditeur','Install / update package':'Installer / mettre à jour le paquet',
    'Selected for next startup':'Sélection pour le prochain démarrage','Publisher not trusted':'Éditeur non approuvé',
    'Remove from selection':'Retirer de la sélection','Retained version':'Version conservée','Select previous version':'Choisir la version précédente',
    'Trusted publishers':'Éditeurs approuvés','Revoke publisher trust':'Révoquer la confiance','Restore publisher trust':'Rétablir la confiance',
    'Unused package versions':'Versions inutilisées','Delete unused version files':'Supprimer les fichiers inutilisés',
    'More unused versions exist. Refresh after cleaning this page.':'D’autres versions inutilisées existent. Actualisez après nettoyage de cette page.',
    'Confirm package change':'Confirmer le changement d’extension',
    'Only the verified unused version files will be deleted. Open instances keep their versions.':'Seuls les fichiers vérifiés de cette version inutilisée seront supprimés. Les instances ouvertes conservent leurs versions.',
    'Select this retained signed version for the next startup?':'Sélectionner cette version signée conservée pour le prochain démarrage ?',
    'Remove this plugin from the next startup selection? Running instances keep their loaded code.':'Retirer cette extension de la sélection au prochain démarrage ? Les instances actives conservent leur code chargé.',
    'This version is still used by an open instance.':'Une instance ouverte utilise encore cette version.',
    'Plugin package settings saved.':'Configuration des paquets enregistrée.'})
translations.update({'New text document':'Nouveau document texte','Save as...':'Enregistrer sous...',
    'The document was closed externally. Local edits were kept in a new draft.':'Le document a été fermé par un autre service. Les modifications locales sont conservées dans un nouveau brouillon.',
    'New workspace file':'Nouveau fichier du projet','Open unsaved':'Ouvrir sans enregistrer',
    'Choose a new file in an existing workspace folder. Existing files are preserved.':'Choisissez un nouveau fichier dans un dossier existant du projet. Les fichiers existants sont conservés.'})
translations.update({'Git repository':'Dépôt Git','Automatic (active file)':'Automatique (fichier actif)',
    'Rescan Git folders':'Rechercher les dépôts Git','Worktrees':'Worktrees',
    'Git folder discovery reached its limit. Automatic selection still follows the active file.':'La recherche des dépôts a atteint sa limite. La sélection automatique suit toujours le fichier actif.',
    'Refresh worktrees':'Actualiser les worktrees','Worktree catalogue unavailable':'Catalogue des worktrees indisponible',
    'Refresh the catalogue before creating or changing a worktree.':'Actualisez le catalogue avant de créer ou modifier un worktree.',
    'Create linked worktree':'Créer un worktree lié','New workspace folder':'Nouveau dossier du projet',
    'Checkout mode':'Mode de checkout','New branch':'Nouvelle branche','Existing branch':'Branche existante',
    'Detached HEAD':'HEAD détachée','Create worktree':'Créer le worktree',
    'The parent folder must exist. Git checkout can execute configured hooks.':'Le dossier parent doit exister. Le checkout Git peut exécuter les hooks configurés.',
    'Worktree lock reason':'Motif du verrouillage','Selected repository':'Dépôt sélectionné',
    'Locked':'Verrouillé','Outside workspace: catalogue only.':'Hors de cet espace de travail : consultation uniquement.',
    'Open in new window':'Ouvrir dans une nouvelle fenêtre','Unlock worktree':'Déverrouiller le worktree',
    'Lock worktree':'Verrouiller le worktree','Remove clean worktree':'Supprimer le worktree propre',
    'Remove linked worktree?':'Supprimer le worktree lié ?',
    'This removes the clean linked checkout. The branch is retained. Close its open documents first.':'Cette action supprime le checkout lié propre. La branche est conservée. Fermez d’abord ses documents ouverts.',
    'Repository changed. Cancel and review the action again.':'Le dépôt a changé. Annulez et vérifiez à nouveau cette action.'})
translations.update({
    'Hierarchy':'Hiérarchie','Call hierarchy':'Hiérarchie des appels','Type hierarchy':'Hiérarchie des types',
    'Symbol hierarchy':'Hiérarchie du symbole','Refresh at caret':'Actualiser au curseur',
    'Inspect symbol at caret':'Examiner le symbole au curseur',
    'Place the caret on a function or type, then choose a hierarchy.':'Placez le curseur sur une fonction ou un type, puis choisissez une hiérarchie.',
    'Callers':'Appelants','Callees':'Fonctions appelées','Base types':'Types parents','Derived types':'Types dérivés',
    'Open symbol':'Ouvrir le symbole','Call sites':'Sites d’appel',
    'Loading hierarchy...':'Chargement de la hiérarchie…','No relationships':'Aucune relation',
    'No hierarchy at this position':'Aucune hiérarchie à cet emplacement',
    'Recursive relationship':'Relation récursive','Hierarchy depth limit reached':'Limite de profondeur atteinte',
    'Retry branch':'Réessayer cette branche',
    'Hierarchy context changed. Refresh at the symbol.':'Le contexte a changé. Actualisez depuis le symbole.',
    'This hierarchy is stale. Refresh before expanding or navigating.':'Cette hiérarchie est périmée. Actualisez avant de la parcourir.',
    'Some hierarchy results were omitted or reached a limit.':'Certains résultats ont été omis ou une limite a été atteinte.',
    'Hierarchy view limit reached. Start again from a symbol.':'La vue a atteint sa limite. Recommencez depuis un symbole.'
})
translations.update({
    'CMake preset actions':'Actions des presets CMake','CMake source directory':'Dossier source CMake',
    'Preset action':'Action du preset','Test preset':'Preset de tests','Package preset':'Preset de paquet',
    'Workflow preset':'Preset de workflow','Preset':'Preset','Select a preset':'Choisir un preset',
    'Refresh CMake presets':'Actualiser les presets CMake','Preview preset steps':'Aperçu des étapes du preset',
    'Minimum CMake':'CMake minimum','Process timeout':'Délai du processus','Declared output paths':'Chemins de sortie déclarés',
    'Step':'Étape','Run reviewed preset':'Exécuter le preset vérifié',
    'Choose a test, package or workflow preset, then preview its steps.':'Choisissez un preset de tests, de paquet ou de workflow, puis affichez ses étapes.',
    'Execution uses trusted project scripts. Failed or cancelled steps may leave outputs.':'L’exécution utilise les scripts du projet. Un échec ou une annulation peut laisser des fichiers produits.',
    'CMake test presets':'Presets de tests CMake','CMake package presets':'Presets de paquets CMake',
    'CMake workflow presets':'Presets de workflow CMake'
})
translations.update({
    'Change annotations':'Annotations des modifications',
    'Confirm this annotated change':'Confirmer cette modification annotée',
    'Confirm the required annotations before applying.':'Confirmez les annotations requises avant d’appliquer.'
})
translations.update({
    'Entire Cargo workspace':'Tout le workspace Cargo','Workspace packages':'Packages du workspace',
    'Select a package':'Choisir un package','Cargo target kind':'Type de cible Cargo',
    'Cargo target name':'Nom de la cible Cargo','Select a Cargo target':'Choisir une cible Cargo',
    'Required Cargo features':'Features Cargo requises',
    'Cargo profile (empty uses Configuration)':'Profil Cargo (vide : Configuration)',
    'Cargo features (one per line)':'Features Cargo (une par ligne)',
    'Declared Cargo features':'Features Cargo déclarées','Add a workspace feature':'Ajouter une feature du workspace',
    'Enable all Cargo features':'Activer toutes les features Cargo',
    'Disable default Cargo features':'Désactiver les features Cargo par défaut',
    'Require unchanged Cargo.lock':'Exiger un Cargo.lock inchangé',
    'Configure refreshes Cargo packages, targets and declared features. Run/debug require a binary or executable example; custom profiles override Configuration.':'Configurer actualise les packages, cibles et features déclarées de Cargo. Exécuter/déboguer demande un binaire ou un exemple exécutable ; un profil personnalisé remplace Configuration.'
})
translations.update({
    'Rust analysis':'Analyse Rust','Rust analysis preferences':'Préférences d’analyse Rust',
    'Apply Cargo options and restart Rust':'Appliquer les options Cargo et redémarrer Rust',
    'Cargo options applied to Rust analysis':'Options Cargo appliquées à l’analyse Rust',
    'Cargo analysis manifest':'Manifeste de l’analyse Cargo',
    'Remove Cargo analysis override':'Retirer les options Cargo de l’analyse',
    'The Cargo override is separate from raw server settings. Apply to confirm changes.':'Les options Cargo sont séparées des réglages bruts du serveur. Appliquez pour valider les changements.',
    'Applies manifest, features, default-feature and offline/locked settings. Package, target and custom compilation profiles remain build controls.':'Applique le manifeste, les features, leurs valeurs par défaut et les options hors ligne/verrouillées. Le package, la cible et les profils personnalisés restent des contrôles de compilation.'
})
translations.update({
    'Rustup toolchain (empty is automatic)':'Toolchain Rustup (vide : automatique)',
    'Rust toolchain':'Toolchain Rust',
    'Uses an installed Rustup toolchain for Cargo. Apply the Cargo options below to use it for Rust analysis too. No installation is requested.':'Utilise une toolchain Rustup installée pour Cargo. Appliquez les options Cargo ci-dessous pour l’utiliser aussi pour l’analyse Rust. Aucune installation n’est demandée.',
    'Rustup was not found. Configure PATH or CYCODE_RUSTUP.':'Rustup est introuvable. Configurez PATH ou CYCODE_RUSTUP.'
})
translations.update({
    'Debugging is available here for Cargo and Java targets.':'Le débogage est disponible ici pour les cibles Cargo et Java.',
    'Java / JDT LS':'Java / JDT LS',
    'Java uses JDT LS with the Microsoft java-debug bundle. Project tasks fills mainClass, classPaths and javaExec after a successful build. Other options: projectName, sourcePaths and vmArgs. JVM attach is not connected yet.':'Java utilise JDT LS avec le module Microsoft java-debug. Les tâches du projet remplissent mainClass, classPaths et javaExec après une compilation réussie. Autres options : projectName, sourcePaths et vmArgs. L’attachement à une JVM reste à connecter.'
})
translations.update({
    'Java uses JDT LS with the Microsoft java-debug bundle. Project tasks fills launch options after a successful build. Attach uses the JDWP host and port below; it does not build or launch the JVM.':'Java utilise JDT LS avec le module Microsoft java-debug. Les tâches du projet remplissent les options de lancement après une compilation réussie. L’attachement utilise l’hôte et le port JDWP ci-dessous ; il ne compile ni ne lance la JVM.',
    'JVM host':'Hôte de la JVM',
    'JDWP port':'Port JDWP',
    'Attach timeout (ms)':'Délai d’attachement (ms)',
    'Attach to JVM':'Attacher à la JVM',
    'JVM target':'Cible JVM',
    'Stop disconnects the debugger and leaves the existing JVM running.':'L’arrêt déconnecte le débogueur et laisse la JVM existante en cours d’exécution.',
    'Maven/Gradle use project-defined builds and runtime classpaths. Java debugging requires JDT LS with the Microsoft java-debug add-on.':'Maven/Gradle utilisent les compilations et les dépendances d’exécution du projet. Le débogage Java nécessite JDT LS et le module Microsoft java-debug.'
})
translations.update({
    'Apply Java code changes':'Appliquer les modifications Java',
    'Cancel Java reload':'Annuler le rechargement Java',
    'Compiling Java changes':'Compilation des modifications Java',
    'Cancelling Java compilation':'Annulation de la compilation Java',
    'Applying Java changes':'Application des modifications Java',
    'Java changes applied':'Modifications Java appliquées',
    'No Java classes to reload':'Aucune classe Java à recharger',
    'Java reload cancelled':'Rechargement Java annulé',
    'Java reload failed':'Échec du rechargement Java',
    'Classes reported by Java debugger':'Classes signalées par le débogueur Java',
    'Java reload compiles the imported JDT workspace, then applies method body changes. Structural changes may require restarting the JVM.':'Le rechargement Java compile l’espace JDT importé, puis applique les modifications du corps des méthodes. Les modifications structurelles peuvent nécessiter de redémarrer la JVM.'
})
translations.update({
    'Data breakpoints':'Points d’arrêt sur données',
    'This debug adapter does not advertise data breakpoints.':'Ce débogueur ne propose pas de points d’arrêt sur données.',
    'Pause the program to inspect or replace data breakpoints.':'Mettez le programme en pause pour examiner ou remplacer les points d’arrêt sur données.',
    'A replacement changes the full list. Inspect every desired target at this pause; installed breakpoints remain active after resuming.':'Un remplacement modifie toute la liste. Examinez chaque cible souhaitée pendant cette pause ; les points d’arrêt installés restent actifs après la reprise.',
    'Variable or expression':'Variable ou expression',
    'Variable context':'Contexte de la variable',
    'Selected stack frame':'Cadre de pile sélectionné',
    'Inspect data breakpoint target':'Examiner la cible du point d’arrêt sur données',
    'The adapter reports persistence support; CyCode keeps data breakpoints within this session.':'Le débogueur indique une prise en charge de la persistance ; CyCode conserve les points d’arrêt sur données dans cette session.',
    'Break on access':'Arrêter lors d’un accès',
    'Adapter default':'Valeur par défaut du débogueur',
    'Read':'Lecture','Write':'Écriture','Read or write':'Lecture ou écriture',
    'Add target to replacement':'Ajouter la cible au remplacement',
    'Replacement list':'Liste de remplacement',
    'New replacement list':'Nouvelle liste de remplacement',
    'Replace data breakpoints':'Remplacer les points d’arrêt sur données',
    'Clear data breakpoints':'Effacer les points d’arrêt sur données',
    'Data breakpoint status':'État des points d’arrêt sur données',
    'Data breakpoint state is uncertain. Pause and replace or clear the full list.':'L’état des points d’arrêt sur données est incertain. Mettez en pause puis remplacez ou effacez toute la liste.',
    'Verification':'Vérification','Verified':'Vérifié','Not verified':'Non vérifié','State unknown':'État inconnu'
})
translations.update({
    'Change value':'Modifier la valeur',
    'Edit value':'Modifier une valeur',
    'Right-click a variable or scope value to edit it. A watched expression can also be assigned when the adapter supports it.':'Faites un clic droit sur une variable ou une valeur de portée pour la modifier. Une expression surveillée peut aussi être affectée si le débogueur le permet.',
    'Edit watched expression':'Modifier l’expression surveillée',
    'Not evaluated for assignment':'Valeur non évaluée pour cette affectation',
    'Target':'Cible',
    'Previous displayed value':'Valeur précédemment affichée',
    'New value':'Nouvelle valeur',
    'This changes the paused target. There is no automatic Undo.':'Cette action modifie la cible en pause. Il n’y a pas d’annulation automatique.',
    'Apply value change':'Appliquer la modification',
    'Select a variable or watched expression first.':'Sélectionnez d’abord une variable ou une expression surveillée.',
    'Checking target bytes':'Vérification des octets de la cible',
    'Applying target change':'Application de la modification à la cible',
    'Target change confirmed':'Modification de la cible confirmée',
    'Partial target change':'Modification partielle de la cible',
    'Target change rejected':'Modification de la cible refusée',
    'Target change unconfirmed':'Modification de la cible non confirmée',
    'Last target change':'Dernière modification de la cible',
    'Memory writes require adapter support for both reading and writing.':'L’écriture mémoire nécessite un débogueur prenant en charge la lecture et l’écriture.',
    'Write target memory':'Écrire dans la mémoire de la cible',
    'Read the target first, then enter replacement hex bytes. The expected bytes are checked again before writing.':'Lisez d’abord la cible, puis saisissez les octets de remplacement en hexadécimal. Les octets attendus sont vérifiés de nouveau avant l’écriture.',
    'Read enough target bytes for this replacement.':'Lisez suffisamment d’octets de la cible pour ce remplacement.',
    'Write these bytes':'Écrire ces octets',
    'Adapter-reported bytes written':'Octets écrits signalés par le débogueur'
})
translations.update({
    'Execution controls':'Contrôles d’exécution',
    'Pause the program to change its execution position.':'Mettez le programme en pause pour modifier sa position d’exécution.',
    'Stepping granularity':'Granularité du pas à pas',
    'Statement':'Instruction source','Source line':'Ligne source','Machine instruction':'Instruction machine',
    'Thread execution':'Exécution des threads','Only selected thread':'Seulement le thread sélectionné',
    'Stepping shortcuts use these options. The thread option also applies to Continue. Unavailable options reset to the adapter default.':'Les raccourcis de pas à pas utilisent ces options. L’option de thread s’applique aussi à Continuer. Les options indisponibles reviennent aux valeurs par défaut du débogueur.',
    'Forward execution':'Exécution en avant','Reverse execution':'Exécution en arrière',
    'Reverse execution requires a debugger with recorded execution support.':'L’exécution en arrière nécessite un débogueur prenant en charge l’historique d’exécution.',
    'Step back':'Pas en arrière','Continue backward':'Continuer en arrière',
    'Restart stack frame':'Redémarrer une frame de pile','Restart selected frame':'Redémarrer la frame sélectionnée',
    'Restarting a frame can repeat program side effects. It is separate from source Undo.':'Redémarrer une frame peut répéter des effets du programme. Cette action est distincte de l’annulation des modifications du code.',
    'Last execution request':'Dernière demande d’exécution','Sending execution request':'Envoi de la demande d’exécution',
    'Execution request accepted':'Demande d’exécution acceptée','Execution outcome unconfirmed':'Résultat de l’exécution non confirmé',
    'Acceptance acknowledges the request. The next stopped event supplies the actual stack and values.':'L’acceptation confirme la réception de la demande. Le prochain arrêt fournit la pile et les valeurs réelles.',
    'The debugger reports all threads resumed.':'Le débogueur indique que tous les threads ont repris.',
    'The debugger reports that some threads remain suspended.':'Le débogueur indique que certains threads restent suspendus.'
})
translations.update({
    'Instruction breakpoints':'Points d’arrêt d’instruction',
    'This debug adapter does not advertise instruction breakpoints.':'Ce débogueur ne propose pas de points d’arrêt d’instruction.',
    'Pause the program to edit instruction breakpoints.':'Mettez le programme en pause pour modifier les points d’arrêt d’instruction.',
    'Right-click an instruction address in Disassembly, or enter an adapter reference below. Changes replace the complete session list; review all entries before applying.':'Faites un clic droit sur une adresse dans le désassemblage, ou saisissez une référence du débogueur ci-dessous. Les modifications remplacent toute la liste de la session ; vérifiez toutes les entrées avant d’appliquer.',
    'New instruction replacement list':'Nouvelle liste d’instructions de remplacement',
    'Clear instruction breakpoints':'Effacer les points d’arrêt d’instruction',
    'Byte offset':'Décalage en octets',
    'Use selected frame instruction':'Utiliser l’instruction de la frame sélectionnée',
    'Add instruction to replacement':'Ajouter l’instruction au remplacement',
    'Instruction replacement draft':'Liste d’instructions à appliquer',
    'The instruction list changed. Start a new replacement list before applying.':'La liste d’instructions a changé. Préparez une nouvelle liste de remplacement avant d’appliquer.',
    'Apply instruction breakpoints':'Appliquer les points d’arrêt d’instruction',
    'Instruction breakpoint status':'État des points d’arrêt d’instruction',
    'Instruction breakpoint state is uncertain. Pause and replace or clear the full list.':'L’état des points d’arrêt d’instruction est incertain. Mettez en pause puis remplacez ou effacez toute la liste.',
    'No instruction breakpoints are confirmed in this session.':'Aucun point d’arrêt d’instruction n’est confirmé dans cette session.',
    'Add instruction breakpoint':'Ajouter un point d’arrêt d’instruction',
    'Copy instruction address':'Copier l’adresse de l’instruction',
    'Actual instruction location':'Emplacement réel de l’instruction',
    'Actual byte offset':'Décalage réel en octets'
})
translations.update({
    'Choose step-in target':'Choisir la cible du pas à pas entrant',
    'Step-in targets':'Cibles du pas à pas entrant',
    'This debug adapter does not advertise step-in targets.':'Ce débogueur ne propose pas de cibles de pas à pas entrant.',
    'Pause on a line containing calls, read the available targets, then select the function to enter. Targets expire when the debug context changes.':'Mettez le programme en pause sur une ligne contenant des appels, lisez les cibles disponibles, puis sélectionnez la fonction à atteindre. Les cibles expirent lorsque le contexte du débogage change.',
    'Read step-in targets':'Lire les cibles du pas à pas entrant',
    'The debugger reported no step-in targets for this frame.':'Le débogueur ne signale aucune cible de pas à pas entrant pour cette frame.',
    'Reported location':'Emplacement signalé',
    'Function':'Fonction','Column':'Colonne',
    'Step into selected target':'Entrer dans la cible sélectionnée',
    'The stepping options in Execution controls also apply to this action.':'Les options de pas à pas des Contrôles d’exécution s’appliquent aussi à cette action.',
    'Read targets again after changing the selected frame or thread.':'Relisez les cibles après avoir changé la frame ou le thread sélectionné.'
})
translations.update({
    'Set next statement':'Déplacer la prochaine instruction',
    'Next statement':'Prochaine instruction',
    'Waiting for the debugger to report the relocated stack.':'En attente de la pile après déplacement signalée par le débogueur.',
    'Moving the next statement can skip or repeat code. It does not execute the intervening code and has no automatic Undo.':'Déplacer la prochaine instruction peut sauter ou répéter du code. Le code intermédiaire n’est pas exécuté et il n’y a pas d’annulation automatique.',
    'This debug adapter does not advertise next-statement targets.':'Ce débogueur ne propose pas de destinations pour déplacer la prochaine instruction.',
    'Pause and select a stack frame with a source before reading destinations.':'Mettez en pause et sélectionnez une frame avec une source avant de lire les destinations.',
    'Selected frame source':'Source de la frame sélectionnée',
    'Source reference':'Référence de source',
    'Destination line':'Ligne de destination',
    'Destination column (0 = unspecified)':'Colonne de destination (0 = non précisée)',
    'Coordinates use one-based source lines and UTF-16 columns in the selected frame source.':'Les coordonnées utilisent des lignes et des colonnes UTF-16 à partir de 1 dans la source de la frame sélectionnée.',
    'Read next-statement targets':'Lire les destinations de la prochaine instruction',
    'The debugger reported no valid destination for this location.':'Le débogueur ne signale aucune destination valide pour cet emplacement.',
    'Destination':'Destination','Set next statement here':'Déplacer la prochaine instruction ici'
})
translations.update({
    'Run to cursor':'Exécuter jusqu’au curseur',
    'Restore source breakpoints':'Restaurer les points d’arrêt du fichier',
    'Resume with a verified temporary breakpoint at the cursor. Existing breakpoints and exceptions can stop the program first.':'Reprenez avec un point d’arrêt temporaire vérifié au curseur. Les points d’arrêt existants et les exceptions peuvent arrêter le programme avant.',
    'Verifying destination breakpoint':'Vérification du point d’arrêt de destination',
    'Running to cursor':'Exécution jusqu’au curseur',
    'The temporary breakpoint will be removed at the next stop.':'Le point d’arrêt temporaire sera retiré au prochain arrêt.',
    'Restoring source breakpoints':'Restauration des points d’arrêt du fichier',
    'Run-to-cursor stop':'Arrêt de l’exécution jusqu’au curseur',
    'Run-to-cursor outcome unconfirmed':'Résultat de l’exécution jusqu’au curseur non confirmé',
    'The original source breakpoint list still needs restoration. Pause the program and wait for outstanding replies before restoring, or stop the debug session.':'La liste d’origine des points d’arrêt doit encore être restaurée. Mettez en pause et attendez les réponses en cours avant de restaurer, ou arrêtez la session de débogage.',
    'The debugger reported the destination breakpoint hit.':'Le débogueur signale que le point d’arrêt de destination a été atteint.',
    'Reported breakpoint IDs do not include the destination.':'Les identifiants de points d’arrêt signalés n’incluent pas la destination.',
    'The debugger did not report whether the destination breakpoint was hit.':'Le débogueur ne précise pas si le point d’arrêt de destination a été atteint.',
    'Pause and select a saved workspace source document. The destination line must be verified without relocation.':'Mettez en pause et sélectionnez un fichier source enregistré dans l’espace de travail. La ligne de destination doit être vérifiée sans déplacement.'
})
translations.update({
    'Submodules':'Sous-modules','Refresh submodules':'Actualiser les sous-modules',
    'Select a submodule to review an explicit action. Updates use the commit recorded by the parent project.':'Sélectionnez un sous-module pour examiner une action explicite. La mise à jour utilise le commit enregistré par le projet principal.',
    'Submodule catalogue unavailable':'Catalogue des sous-modules indisponible',
    'Refresh before changing submodules.':'Actualisez avant de modifier les sous-modules.',
    'No configured submodules in this repository.':'Aucun sous-module configuré dans ce dépôt.',
    'At recorded commit':'Au commit enregistré','Different commit':'Commit différent',
    'Not checked out':'Fichiers non récupérés','Conflicted gitlink':'Gitlink en conflit',
    'Not a staged gitlink':'Absent des gitlinks de l’index','Submodule unavailable':'Sous-module indisponible',
    'Local modifications':'Modifications locales','Recorded commit':'Commit enregistré',
    'Register submodule':'Enregistrer le sous-module',
    'Synchronize submodule URL':'Synchroniser l’URL du sous-module',
    'Update to recorded commit':'Passer au commit enregistré',
    'Review submodule action':'Examiner l’action sur le sous-module',
    'Checkout detaches HEAD at this commit. Close documents in the submodule and keep its working tree clean. Git may execute configured hooks.':'La récupération détache HEAD sur ce commit. Fermez les documents du sous-module et conservez son arborescence sans modification. Git peut exécuter les hooks configurés.',
    'Allow clone/fetch':'Autoriser le clonage et la récupération',
    'Register the configured URL locally without downloading files.':'Enregistrer l’URL configurée localement sans télécharger de fichiers.',
    'Copy the URL from .gitmodules to the registered submodule configuration.':'Copier l’URL de .gitmodules dans la configuration du sous-module enregistré.',
    'Submodule state changed. Review the action again.':'L’état du sous-module a changé. Examinez de nouveau l’action.',
    'Apply submodule action':'Appliquer l’action sur le sous-module',
    'Cancel submodule action':'Annuler l’action sur le sous-module'
})
translations.update({
    'Add submodule':'Ajouter un sous-module','New Git submodule':'Nouveau sous-module Git',
    'Repository address':'Adresse du dépôt','New repository-relative folder':'Nouveau dossier relatif au dépôt',
    'Module name (optional)':'Nom du module (facultatif)','Remote branch (optional)':'Branche distante (facultative)',
    'Use a new folder with an existing parent. Close .gitmodules and destination documents; keep .gitmodules unchanged. Use Git credentials or SSH authentication.':'Utilisez un nouveau dossier dont le parent existe. Fermez .gitmodules et les documents de destination ; conservez .gitmodules sans modification. Utilisez les identifiants Git ou l’authentification SSH.',
    'Preview submodule addition':'Aperçu de l’ajout du sous-module',
    'Submodule addition preview':'Aperçu de l’ajout du sous-module',
    'Module name':'Nom du module','Remote branch':'Branche distante','Remote default branch':'Branche distante par défaut',
    'The remote commit is resolved during cloning. This creates a checkout and stages .gitmodules plus its gitlink. Existing staged files stay staged; no commit is created.':'Le commit distant est résolu lors du clonage. Cela crée une copie de travail et prépare .gitmodules ainsi que son gitlink dans l’index. Les fichiers déjà préparés le restent ; aucun commit n’est créé.',
    'Cloning may contact the remote and execute configured Git helpers or hooks. Failed additions keep partial files; inspect them before trying again.':'Le clonage peut contacter le dépôt distant et exécuter les outils ou hooks Git configurés. En cas d’échec, les fichiers partiels sont conservés ; examinez-les avant de réessayer.',
    'Clone and stage submodule':'Cloner et préparer le sous-module','Cancel submodule addition':'Annuler l’ajout du sous-module'
})
translations.update({
    'Review submodule deinitialization':'Examiner la désactivation du sous-module',
    'Deinitialization removes this checkout and its local registration. The project gitlink, .gitmodules and Git repository remain.':'La désactivation retire cette copie de travail et son enregistrement local. Le gitlink du projet, .gitmodules et le dépôt Git sont conservés.',
    'Close submodule documents and .gitmodules. Modified, untracked, ignored files, hidden index flags and nested submodules block this action.':'Fermez les documents du sous-module et .gitmodules. Les fichiers modifiés, non suivis ou ignorés, les indicateurs d’index masquant des changements et les sous-modules imbriqués bloquent cette action.',
    'Preview deinitialization again':'Actualiser l’aperçu de désactivation',
    'Tracked files to remove':'Fichiers suivis à retirer',
    'This local Git action has no editor Undo. Reinitializing the retained repository is a separate action. Inspect partial outcomes before retrying.':'Cette action Git locale ne peut pas être annulée par l’éditeur. Réinitialiser le dépôt conservé est une action distincte. Examinez les résultats partiels avant de réessayer.',
    'Remove reviewed submodule checkout':'Retirer la copie de travail examinée'
})
translations.update({
    'Edit submodule settings':'Modifier les paramètres du sous-module',
    'Setting':'Paramètre','New value':'Nouvelle valeur','Tracking branch':'Branche de suivi',
    "An empty tracking branch uses remote HEAD; '.' follows the parent branch name. The recorded-commit update keeps using the gitlink.":"Une branche de suivi vide utilise le HEAD distant ; « . » reprend le nom de la branche du parent. La mise à jour vers le commit enregistré continue d’utiliser le gitlink.",
    'Close .gitmodules; URL changes also require closed submodule documents. Existing metadata edits are preserved. New changes remain unstaged, without checkout or fetch.':'Fermez .gitmodules ; les changements d’URL nécessitent aussi de fermer les documents du sous-module. Les modifications de métadonnées existantes sont conservées. Les nouveaux changements restent hors index, sans changement de checkout ni récupération.',
    'Preview submodule settings':'Aperçu des paramètres du sous-module',
    'Submodule settings preview':'Aperçu des paramètres du sous-module',
    'Previous configured value':'Ancienne valeur configurée',
    'The previous value is hidden because it is sensitive or unsupported.':'L’ancienne valeur est masquée car elle est sensible ou non prise en charge.',
    'No configured value':'Aucune valeur configurée',
    'Git also synchronizes local URL settings according to its active-submodule configuration. No remote connection is requested.':'Git synchronise aussi les URL locales selon sa configuration des sous-modules actifs. Aucune connexion distante n’est demandée.',
    'A local tracking-branch override exists and remains in effect. This action edits .gitmodules only.':'Une branche de suivi est définie localement et reste prioritaire. Cette action modifie uniquement .gitmodules.',
    'The tracking branch is already configured as requested.':'La branche de suivi possède déjà la configuration demandée.'
})
translations.update({
    'Saved Git repository is unavailable or changed. Using automatic selection.':'Le dépôt Git mémorisé est indisponible ou a changé. La sélection automatique est utilisée.',
    'Move linked worktree':'Déplacer le worktree lié','Repair moved worktree':'Réparer un worktree déplacé',
    'Review worktree move':'Examiner le déplacement du worktree','Review worktree link repair':'Examiner la réparation des liens du worktree',
    'Moved worktree folder':'Dossier du worktree déplacé',
    'The destination must be new and its parent must exist. Local files and staged changes move with the checkout. Close its documents first.':'La destination doit être nouvelle et son parent doit exister. Les fichiers locaux et les changements préparés accompagnent le checkout. Fermez d’abord ses documents.',
    'Select the existing folder moved manually from this missing path. Repair requires the main checkout and all linked worktrees inside this workspace, with closed linked documents.':'Sélectionnez le dossier existant déplacé manuellement depuis ce chemin absent. La réparation nécessite le checkout principal et tous les worktrees liés dans cet espace de travail, avec leurs documents fermés.',
    'No force, checkout, staging, commit or Git transport is requested. Partial outcomes require inspection; this has no editor Undo.':'Aucun forçage, checkout, ajout à l’index, commit ou transport Git n’est demandé. Les résultats partiels doivent être examinés ; l’éditeur ne peut pas annuler cette action.',
    'Worktree state changed. Cancel and review the action again.':'L’état du worktree a changé. Annulez puis examinez de nouveau l’action.',
    'Preview worktree maintenance':'Aperçu de la maintenance du worktree','Verified worktree paths':'Chemins des worktrees vérifiés',
    'Git repair can update linking files in every listed worktree. Other missing, external, locked or unreviewed links block this action.':'Git peut réparer les fichiers de liaison dans chaque worktree affiché. Les autres liens absents, externes, verrouillés ou non vérifiés bloquent cette action.',
    'Apply worktree maintenance':'Appliquer la maintenance du worktree','Cancel worktree maintenance':'Annuler la maintenance du worktree'
})
translations.update({
    'Refresh conflict catalogue':'Actualiser les conflits',
    'Text':'Texte','Unsupported':'Non pris en charge',
    'Choose versions for each repository-relative path. For a rename, review all affected paths together; rename relationships are not inferred.':'Choisissez une version pour chaque chemin relatif au dépôt. Pour un renommage, examinez tous les chemins concernés ensemble ; leurs relations ne sont pas déduites automatiquement.',
    'Index-only resolution preserves local files and open buffers. Staged content can differ from the editor, including conflict markers. A deletion removes only the index entry.':'La résolution dans l’index conserve les fichiers locaux et les documents ouverts. Le contenu préparé peut différer de l’éditeur, y compris ses marqueurs de conflit. Une suppression retire uniquement l’entrée de l’index.',
    'Ours and theirs name Git index stages 2 and 3; during rebase their branch roles can be reversed.':'Nos versions et leurs versions désignent les étapes 2 et 3 de l’index Git ; pendant un rebase, leurs rôles de branche peuvent être inversés.',
    'No unresolved index entries.':'Aucune entrée non résolue dans l’index.',
    'Index resolution':'Résolution dans l’index','Keep unresolved':'Laisser non résolu',
    'Stage ours':'Préparer notre version','Stage theirs':'Préparer leur version','Stage base':'Préparer la base','Stage deletion':'Préparer la suppression',
    'Inspect versions':'Examiner les versions','Absent':'Absent',
    'Binary or unsupported encoding':'Binaire ou encodage non pris en charge','Too large for text preview':'Trop volumineux pour l’aperçu texte',
    'Open working text conflict':'Ouvrir le conflit texte local','Preview index resolution':'Aperçu de la résolution dans l’index',
    'Reviewed index changes':'Changements examinés dans l’index',
    'This action stages Git objects without checkout, text conversion or editor Undo. Refresh after an uncertain result; do not automatically retry.':'Cette action prépare les objets Git sans modifier les fichiers locaux, sans conversion de texte ni annulation dans l’éditeur. Actualisez après un résultat incertain ; ne relancez pas automatiquement.',
    'Apply reviewed index resolution':'Appliquer la résolution examinée','Clear conflict choices':'Effacer les choix de résolution',
    'Selected versions staged; working files unchanged.':'Versions sélectionnées préparées ; fichiers locaux conservés.'
})
translations.update({
    'Synchronize working file':'Synchroniser le fichier local',
    'Working file updated':'Fichier local mis à jour','Working file restored':'Fichier local restauré',
    'Working file requires inspection':'Le fichier local doit être vérifié',
    'Copy exact staged bytes or remove a path absent from the index. Existing content is retained in project trash. Close affected text documents first.':'Copiez les octets exacts préparés dans Git ou retirez un chemin absent de l’index. Le contenu existant est conservé dans la corbeille du projet. Fermez d’abord les documents texte concernés.',
    'One file per transaction. Git filters and line-ending conversion are not applied; the index stays unchanged. This file action has a separate Undo.':'Un fichier par transaction. Les filtres Git et conversions de fins de ligne ne sont pas appliqués ; l’index reste inchangé. Cette action possède une annulation distincte.',
    'Resolved paths':'Chemins résolus','Repository-relative working file':'Fichier local relatif au dépôt',
    'Copy staged bytes':'Copier les octets préparés','Remove untracked working file':'Retirer le fichier local absent de l’index',
    'Preview working-file action':'Aperçu de l’action sur le fichier',
    'Close affected documents and prepare a fresh preview.':'Fermez les documents concernés puis préparez un nouvel aperçu.',
    'Apply reviewed working-file action':'Appliquer l’action examinée sur le fichier',
    'Cancel working-file preview':'Annuler l’aperçu du fichier',
    'Original file backup':'Sauvegarde du fichier original','Undo displaced-file backup':'Sauvegarde du fichier déplacé par l’annulation',
    'Open project trash':'Ouvrir la corbeille du projet','Undo working-file synchronization':'Annuler la synchronisation du fichier',
    'Working-file action is incomplete. Inspect the receipt and project trash before continuing.':'L’action sur le fichier est incomplète. Examinez le résultat et la corbeille du projet avant de continuer.',
    'Working-file action completed; Git index retained.':'Action sur le fichier terminée ; index Git conservé.'
})
translations.update({
    'Merge':'Fusion','No merge is active.':'Aucune fusion en cours.',
    'Finish the other Git operation first.':'Terminez d’abord l’autre opération Git.',
    'This merge has an autostash; finish it with Git before using these actions.':'Cette fusion possède un autostash ; terminez-la avec Git avant d’utiliser ces actions.',
    'Close open Git metadata documents.':'Fermez les documents de métadonnées Git ouverts.',
    'HEAD changed since the merge started; inspect Git state.':'HEAD a changé depuis le début de la fusion ; vérifiez l’état Git.',
    'Resolve all index conflicts first.':'Résolvez d’abord tous les conflits de l’index.',
    'Working files differ from the index. Review or synchronize them first.':'Les fichiers locaux diffèrent de l’index. Examinez-les ou synchronisez-les d’abord.',
    'Save or reconcile modified repository documents.':'Enregistrez ou réconciliez les documents modifiés du dépôt.',
    'Clear skip-worktree or assume-unchanged flags before committing here.':'Retirez les indicateurs skip-worktree ou assume-unchanged avant de créer le commit ici.',
    'Clear skip-worktree, assume-unchanged or intent-to-add flags before committing here.':'Retirez les indicateurs skip-worktree, assume-unchanged ou intent-to-add avant de créer le commit ici.',
    'This workflow completes a merge with one incoming parent.':'Ce parcours termine une fusion avec un seul parent entrant.',
    'Another Git operation is active; these merge actions are unavailable.':'Une autre opération Git est active ; ces actions de fusion sont indisponibles.',
    'Merge action completed.':'Action de fusion terminée.','Refresh merge state':'Actualiser l’état de fusion',
    'Merge intent receipt':'Reçu d’intention de fusion','Incoming merge parent':'Parent entrant de la fusion',
    'Unresolved paths':'Chemins non résolus','Review merge conflicts':'Examiner les conflits de fusion',
    'Complete merge commit':'Créer le commit de fusion','Quit merge tracking':'Quitter le suivi de fusion',
    'Merge commit message':'Message du commit de fusion','Run configured Git commit hooks':'Exécuter les hooks Git de commit configurés',
    'The complete staged index becomes an unsigned merge commit. Review all paths. Untracked files stay outside it.':'L’index préparé complet devient un commit de fusion non signé. Examinez tous les chemins. Les fichiers non suivis restent en dehors.',
    'Quit removes merge bookkeeping while keeping working files and index entries. The next ordinary commit will not retain these merge parents.':'Cette sortie retire le suivi de fusion en conservant les fichiers locaux et l’index. Le prochain commit ordinaire ne conservera pas ces parents de fusion.',
    'Configured hooks can run project tools and network commands. Close repository documents before enabling them.':'Les hooks configurés peuvent lancer des outils du projet et des commandes réseau. Fermez les documents du dépôt avant de les activer.',
    'Close repository documents before running hooks.':'Fermez les documents du dépôt avant d’exécuter les hooks.',
    'Review staged merge diff':'Examiner les différences préparées de la fusion','Staged merge paths':'Chemins préparés de la fusion',
    'Preview merge action':'Aperçu de l’action de fusion','Reviewed merge action':'Action de fusion examinée',
    'An intent receipt is retained before dispatch. Inspect actual Git state after errors or uncertainty; this action has no editor Undo.':'Un reçu d’intention est conservé avant l’exécution. Vérifiez l’état Git réel après une erreur ou un résultat incertain ; cette action ne peut pas être annulée dans l’éditeur.',
    'Apply reviewed merge action':'Appliquer l’action de fusion examinée','Cancel merge preview':'Annuler l’aperçu de fusion'
})
translations.update({
    'Replay':'Reprise de commits','Refresh replay state':'Actualiser la reprise de commits',
    'Replay intent receipt':'Reçu d’intention de reprise','Replay action completed.':'Action de reprise terminée.',
    'Another Git operation is active.':'Une autre opération Git est active.',
    'This action requires an active single-commit cherry-pick or revert.':'Cette action exige un cherry-pick ou revert actif portant sur un seul commit.',
    'HEAD has no commit.':'HEAD ne possède aucun commit.',
    'Close repository documents before replaying commits.':'Fermez les documents du dépôt avant de reprendre des commits.',
    'Index changes must be committed or removed before starting a replay.':'Créez un commit ou retirez les modifications de l’index avant de démarrer une reprise.',
    'Cherry-pick in progress':'Cherry-pick en cours','Revert in progress':'Revert en cours','No replay is active.':'Aucune reprise de commit n’est active.',
    'Review replay conflicts':'Examiner les conflits de reprise','Cherry-pick commit':'Reprendre le commit (cherry-pick)',
    'Revert commit':'Inverser le commit (revert)','Continue resolved replay':'Poursuivre la reprise résolue',
    'Quit replay tracking':'Quitter le suivi de reprise','Source commit ID':'Identifiant du commit source',
    'Complete commit hash; choose from History':'Hash complet du commit ; choisir dans l’historique',
    'Quit keeps working files and index entries. It does not restore the pre-replay state.':'Cette sortie conserve les fichiers locaux et l’index. Elle ne restaure pas l’état précédant la reprise.',
    'Replay creates an unsigned commit. Hooks are disabled; trusted Git filters or merge drivers may run. Close repository documents first.':'La reprise crée un commit non signé. Les hooks sont désactivés ; les filtres Git ou pilotes de fusion de confiance peuvent s’exécuter. Fermez d’abord les documents du dépôt.',
    'Review staged replay diff':'Examiner les différences préparées de la reprise','Preview replay action':'Aperçu de l’action de reprise',
    'Reviewed replay action':'Action de reprise examinée','Review source commit patch':'Examiner le patch du commit source',
    'Source changes are not a simulation of the resulting working tree. Conflicts can stop the operation; review actual state before continuing.':'Les modifications sources ne simulent pas le résultat dans les fichiers locaux. Des conflits peuvent arrêter l’opération ; examinez l’état réel avant de poursuivre.',
    'Apply reviewed replay action':'Appliquer l’action de reprise examinée','Cancel replay preview':'Annuler l’aperçu de reprise'
})
translations.update({
    'Cook and package Unreal content':'Cuisson et packaging du contenu Unreal','Operation':'Opération',
    'Cook content':'Cuire le contenu','Package and archive':'Créer le package et l’archive','Game target':'Cible du jeu',
    'Maps to cook':'Cartes à cuire','Build game code':'Compiler le code du jeu','Pak containers':'Conteneurs Pak',
    'IoStore containers':'Conteneurs IoStore','Compress package':'Compresser le package','Deadline (seconds)':'Délai maximal (secondes)',
    'Use one /Game/Folder/Map package per line, without .umap. Leave empty to use project packaging defaults. Platform and configuration come from the Unreal settings above.':'Indiquez un package /Game/Dossier/Carte par ligne, sans .umap. Laissez vide pour utiliser les réglages de packaging du projet. La plateforme et la configuration proviennent des paramètres Unreal ci-dessus.',
    'AutomationTool executes trusted project rules and content tools. Existing cook/build caches can change; packaging uses a new staging/archive folder. The packaged game is not launched.':'AutomationTool exécute les règles et outils de contenu de confiance du projet. Les caches existants de cuisson et de compilation peuvent changer ; le packaging utilise un nouveau dossier de préparation et d’archive. Le jeu créé n’est pas lancé.',
    'Cooking and packaging currently require a desktop target platform.':'La cuisson et le packaging exigent actuellement une plateforme cible de bureau.',
    'Preview Unreal automation':'Aperçu de l’automatisation Unreal','Archive directory':'Dossier d’archive',
    'Reviewed AutomationTool arguments':'Arguments AutomationTool examinés',
    'The preview expires after ten minutes and permits one attempt. Intent, output log and result remain in the workspace after errors or cancellation; inspect them before creating a new preview.':'L’aperçu expire après dix minutes et autorise une seule tentative. L’intention, le journal et le résultat restent dans l’espace de travail après une erreur ou une annulation ; examinez-les avant de créer un nouvel aperçu.',
    'Run reviewed Unreal automation':'Exécuter l’automatisation Unreal examinée','Cancel automation preview':'Annuler l’aperçu d’automatisation',
    'Last automation output':'Dernière sortie d’automatisation','Copy automation output path':'Copier le chemin de sortie d’automatisation',
    'Open automation log':'Ouvrir le journal d’automatisation',
    'An archive file was observed. Package installation remains unverified.':'Un fichier d’archive a été observé. L’installation du package reste à vérifier.',
    'No archive file was confirmed for this result.':'Aucun fichier d’archive n’a été confirmé pour ce résultat.'
})
translations['Task finished with unconfirmed results.']='La tâche est terminée avec des résultats non confirmés.'
translations.update({
    'Unreal launch profile':'Profil de lancement Unreal','Launch mode':'Mode de lancement',
    'Unreal Editor':'Éditeur Unreal','Standalone game':'Jeu autonome','Uncooked server':'Serveur sans cuisson',
    'Startup map':'Carte de démarrage','Launch options':'Options de lancement','Unreal log window':'Fenêtre de journal Unreal',
    'Build before debugging':'Compiler avant le débogage','Listen server':'Serveur d’écoute','Windowed game':'Jeu en fenêtre',
    'Game width':'Largeur du jeu','Game height':'Hauteur du jeu','Game port':'Port du jeu',
    'Use an existing /Game/Folder/Map without .umap, or leave empty for the project default. Game and server modes use uncooked content through the installed editor.':'Indiquez une carte existante /Game/Dossier/Carte sans .umap, ou laissez vide pour la carte par défaut du projet. Les modes jeu et serveur utilisent le contenu sans cuisson via l’éditeur installé.',
    'Development and DebugGame use the current host platform. Building before debugging requires the Editor target selected above; disable it for content-only projects.':'Development et DebugGame utilisent la plateforme de cet ordinateur. La compilation avant débogage exige une cible Editor sélectionnée ci-dessus ; désactivez-la pour les projets sans code.',
    'Preview Unreal launch profile':'Aperçu du profil de lancement Unreal','Reviewed Unreal launch arguments':'Arguments de lancement Unreal examinés',
    'Applying saves the shared debug profile and opens Debugger. Use Run there without compilation, or Debug with the selected build option. No process starts while applying.':'L’application enregistre le profil de débogage commun et ouvre le débogueur. Utilisez Exécuter sans compilation, ou Déboguer avec l’option de compilation choisie. Aucun processus ne démarre pendant cet enregistrement.',
    'Use this Unreal launch profile':'Utiliser ce profil de lancement Unreal','Cancel launch preview':'Annuler l’aperçu de lancement'
})
translations['Unreal launch profile prepared.']='Profil de lancement Unreal préparé.'
translations['Bound Unreal build']='Compilation Unreal liée'
translations['Detach Unreal launch settings']='Détacher les paramètres de lancement Unreal'
translations['Load bound Unreal build settings']='Charger les paramètres de compilation Unreal liés'
translations.update({
    'Unreal build outputs':'Fichiers produits par Unreal','Inspect bound launch outputs':'Inspecter les fichiers du profil de lancement lié',
    'Libraries':'Bibliothèques','Module manifests':'Manifestes de modules','Unreal output inspection finished.':'Inspection des fichiers produits par Unreal terminée.',
    'Inspect Unreal build outputs':'Inspecter les fichiers produits par Unreal','Last output inspection':'Dernière inspection des fichiers produits',
    'Inspect the selected Editor receipt, project libraries and module manifests without running Unreal.':'Inspectez le reçu de compilation Editor sélectionné, les bibliothèques du projet et les manifestes de modules sans lancer Unreal.',
    'Inspected output metadata and files passed the scoped checks.':'Les métadonnées et fichiers inspectés ont satisfait les contrôles du périmètre.',
    'Build outputs are missing, mismatched or not fully inspected.':'Des fichiers produits sont absents, incompatibles ou leur inspection est incomplète.',
    'This checks project Editor outputs, not every engine/plugin dependency or binary ABI. Results are observations; actual launch rechecks them.':'Ces contrôles portent sur les fichiers Editor du projet, sans couvrir toutes les dépendances moteur/plugins ni l’ABI binaire. Les résultats sont des observations, revérifiées au lancement.',
    'Copy output inspection':'Copier l’inspection des fichiers produits','Inspected project files':'Fichiers du projet inspectés'
})
translations.update({
    'Live build output':'Sortie de compilation en direct','Copy live build output':'Copier la sortie en direct',
    'Only the latest build messages are retained here.':'Seuls les derniers messages de compilation sont conservés ici.',
    'The final result is returned to the build caller.':'Le résultat final est renvoyé à l’appelant de la compilation.',
    'Finished':'Terminé','Running':'En cours'
})
translations.update({
    'Native build output':'Sortie de compilation native','Project task output':'Sortie des tâches de projet',
    'Cancelling task...':'Annulation de la tâche…',
    'Next diagnostic':'Diagnostic suivant','Previous diagnostic':'Diagnostic précédent',
    'Next diagnostic in current file':'Diagnostic suivant dans le fichier courant',
    'Previous diagnostic in current file':'Diagnostic précédent dans le fichier courant',
    'Selected diagnostic':'Diagnostic sélectionné','No matching diagnostic location':'Aucun emplacement de diagnostic correspondant'
})
translations.update({'Copy diagnostic': 'Copier le diagnostic', 'Last build result': 'Dernier résultat de compilation', 'Inactive language server': 'Serveur de langage inactif', 'Current document': 'Document courant', 'Position not verified': 'Position non vérifiée', 'Awaiting diagnostics for the current document.': 'En attente des diagnostics du document courant.', 'Errors': 'Erreurs', 'Warnings': 'Avertissements', 'Information': 'Informations'})
translations.update({'Analysis configuration is changing':'La configuration de l’analyse est en cours de modification'})
translations.update({'Structure': 'Structure', 'No document structure': 'Aucune structure de document', 'Open a source file with a language server.': 'Ouvrez un fichier source disposant d’un serveur de langage.', 'Enable a language server that provides document symbols.': 'Activez un serveur de langage fournissant les symboles du document.', 'Refresh structure': 'Actualiser la structure', 'Locate current symbol': 'Localiser le symbole courant', 'Follow caret': 'Suivre le curseur', 'Alphabetical order': 'Ordre alphabétique', 'Reading document structure...': 'Lecture de la structure du document…', 'Structure limited to the reported symbol budget.': 'La structure est limitée au budget de symboles.', 'Invalid symbols ignored': 'Symboles invalides ignorés', 'No document symbols': 'Aucun symbole dans le document', 'The language server returned no document symbols.': 'Le serveur de langage n’a renvoyé aucun symbole pour ce document.', 'No matching symbols': 'Aucun symbole correspondant', 'Current symbol': 'Symbole courant', ' (deprecated)': ' (obsolète)', 'Automatic (active document)': 'Automatique (document actif)', 'Missing language server': 'Serveur de langage absent', 'Symbol language server': 'Serveur de langage pour les symboles', 'Configure and enable a server with workspace symbol support.': 'Configurez et activez un serveur prenant en charge les symboles du projet.', 'Symbol': 'Symbole', 'File': 'Fichier', 'Module': 'Module', 'Namespace': 'Espace de noms', 'Package': 'Paquet', 'Class': 'Classe', 'Method': 'Méthode', 'Property': 'Propriété', 'Field': 'Champ', 'Constructor': 'Constructeur', 'Enum': 'Énumération', 'Interface': 'Interface', 'Function': 'Fonction', 'Variable': 'Variable', 'Constant': 'Constante', 'String': 'Chaîne', 'Number': 'Nombre', 'Boolean': 'Booléen', 'Array': 'Tableau', 'Object': 'Objet', 'Key': 'Clé', 'Null': 'Null', 'Enum member': 'Membre d’énumération', 'Struct': 'Structure', 'Event': 'Événement', 'Operator': 'Opérateur', 'Type parameter': 'Paramètre de type'})
translations.update({'Refresh symbols':'Actualiser les symboles', 'Location on selection':'Position à la sélection', 'Resolving symbol location...':'Résolution de la position du symbole…'})
translations.update({'Go to declaration':'Aller à la déclaration','Go to type definition':'Aller à la définition du type','Go to implementation':'Aller à l’implémentation','Refresh at caret':'Actualiser au curseur','Navigation context changed. Run the query again.':'Le contexte de navigation a changé. Relancez la recherche.','Use Navigate to find definitions, declarations, types, implementations or references.':'Utilisez Navigation pour rechercher les définitions, déclarations, types, implémentations ou références.','Invalid locations ignored':'Emplacements invalides ignorés','Unavailable locations':'Emplacements indisponibles','Previous location':'Emplacement précédent','Next location':'Emplacement suivant','Copy locations':'Copier les emplacements','No matching locations':'Aucun emplacement correspondant'})
translations.update({'Occurrences': 'Occurrences', 'Highlight occurrences at caret': 'Surligner les occurrences au curseur', 'Next reported occurrence': 'Occurrence signalée suivante', 'Previous reported occurrence': 'Occurrence signalée précédente', 'Select reported occurrences': 'Sélectionner les occurrences signalées', 'Clear occurrence highlights': 'Effacer les occurrences surlignées', 'Automatic occurrence highlights': 'Surlignage automatique des occurrences', 'Text occurrences': 'Occurrences textuelles', 'Read occurrences': 'Lectures', 'Write occurrences': 'Écritures', 'highlighted spans': 'plages surlignées', 'Occurrence query or display budget reached.': 'La limite de recherche ou d’affichage des occurrences est atteinte.', 'Invalid occurrences ignored': 'Occurrences invalides ignorées', 'Open a document with document-highlight support, then highlight at caret.': 'Ouvrez un document avec prise en charge des occurrences, puis surlignez au curseur.', 'Navigation and selection use the kind filters; the text filter only filters this list.': 'La navigation et la sélection utilisent les types ; le filtre textuel ne concerne que cette liste.', 'Incomplete occurrence report cannot replace selections.': 'Un rapport d’occurrences incomplet ne peut pas remplacer les sélections.'})
translations.update({'Application updates': 'Mises à jour de l’application', 'Check for updates': 'Rechercher des mises à jour', 'Open GitHub Releases': 'Ouvrir les versions GitHub', 'Check for updates at startup': 'Vérifier les mises à jour au démarrage', 'Skipped version': 'Version ignorée', 'Reset skipped version': 'Réinitialiser la version ignorée', 'Downloading update...': 'Téléchargement de la mise à jour…', 'Checking GitHub Releases...': 'Vérification des versions GitHub…', 'Cancel update request': 'Annuler la demande de mise à jour', 'Update available': 'Mise à jour disponible', 'View this release': 'Voir cette version', 'Install update and restart': 'Installer la mise à jour et redémarrer', 'Download update': 'Télécharger la mise à jour', 'Application update available': 'Une mise à jour de l’application est disponible', 'Later': 'Plus tard', 'Skip this version': 'Ignorer cette version', 'Private releases use CYCODE_UPDATE_TOKEN, GH_TOKEN or GITHUB_TOKEN from the environment. Credentials are never saved in preferences.': 'Les versions privées utilisent CYCODE_UPDATE_TOKEN, GH_TOKEN ou GITHUB_TOKEN dans l’environnement. Les identifiants ne sont jamais enregistrés dans les préférences.', 'Automatic installation currently supports Windows x64 portable packages. Use GitHub Releases for other platforms.': 'L’installation automatique prend actuellement en charge les distributions portables Windows x64. Utilisez les versions GitHub pour les autres plateformes.', 'You are using the latest available stable version.': 'Vous utilisez la dernière version stable disponible.', 'This copy is a development checkout or a legacy package. Download a current portable release to enable installation updates.': 'Cette copie est une version de développement ou une ancienne distribution. Téléchargez une distribution portable récente pour activer l’installation des mises à jour.', 'This release has no compatible verified update archive. Open the release to download it manually.': 'Cette version ne possède pas d’archive de mise à jour compatible et vérifiée. Ouvrez la version pour la télécharger manuellement.', 'Download verified. Installation begins only after the normal save and close flow, once all instances of this copy have closed.': 'Téléchargement vérifié. L’installation débute après l’enregistrement et la fermeture habituels, une fois toutes les instances de cette copie fermées.'})
translations.update({'Update cancelled':'Mise à jour annulée'})
translations.update({'Refresh downloaded updates': 'Actualiser les mises à jour téléchargées', 'Verifying downloaded updates...': 'Vérification des mises à jour téléchargées…', 'Cancelling update request...': 'Annulation de la demande de mise à jour…', 'Invalid cached update records ignored': 'Entrées de mise à jour invalides ignorées', 'Update cache inspection reached its budget. Older entries may be omitted.': 'La limite d’inspection du cache de mises à jour est atteinte. Des entrées anciennes peuvent être omises.', 'Update history': 'Historique des mises à jour', 'No recorded installation attempts.': 'Aucune tentative d’installation enregistrée.', 'Installation completed': 'Installation terminée', 'Installed; restart failed': 'Installée ; échec du redémarrage', 'Waiting for instances or interrupted': 'En attente des instances ou interrompue', 'Installation failed': 'Échec de l’installation', 'Copy update report': 'Copier le rapport de mise à jour', 'Installation errors are kept in update-error.txt beside the downloaded archive. Backups remain in data/updates.': 'Les erreurs d’installation sont conservées dans update-error.txt à côté de l’archive téléchargée. Les sauvegardes restent dans data/updates.', 'Verified downloaded update': 'Mise à jour téléchargée et vérifiée', 'Download newer update': 'Télécharger la version plus récente', 'GitHub reports no newer stable release. The previously downloaded update remains available for explicit installation.': 'GitHub ne signale aucune version stable plus récente. La mise à jour déjà téléchargée reste disponible pour une installation explicite.'})
translations.update({'The downloaded package differs from the current release. Download it again.':'Le fichier téléchargé diffère de la version actuelle. Téléchargez-le à nouveau.'})
translations.update({'Previous installation restored': 'Installation précédente restaurée', 'CyCode update recovery': 'Récupération de la mise à jour de CyCode', 'To recover an interrupted update, run the staged CyCodeUpdater.exe inside data/updates/download-... . Keep pending.json and all backups.': 'Pour récupérer une mise à jour interrompue, lancez le CyCodeUpdater.exe conservé dans data/updates/download-... . Conservez pending.json et toutes les sauvegardes.', 'No interrupted installation is pending. Open CyCode to install downloaded updates.': 'Aucune installation interrompue n’est en attente. Ouvrez CyCode pour installer les mises à jour téléchargées.', 'Installation': 'Installation', 'Managed files to restore': 'Fichiers de l’application à restaurer', 'New files to retain outside the installation': 'Nouveaux fichiers à conserver hors de l’installation', 'Files already in their previous state': 'Fichiers déjà dans leur état précédent', 'Recovery conflicts': 'Conflits de récupération', 'Local changes or missing backups prevent recovery. No installation files were changed.': 'Des modifications locales ou des sauvegardes manquantes empêchent la récupération. Aucun fichier de l’installation n’a été modifié.', 'Restore the previous installation? Projects and user data are preserved. Close all CyCode instances before continuing.': 'Restaurer l’installation précédente ? Les projets et données utilisateur sont conservés. Fermez toutes les instances de CyCode avant de continuer.', 'The previous installation was restored. Original backups were retained. Restart CyCode now?': 'L’installation précédente a été restaurée. Les sauvegardes d’origine ont été conservées. Redémarrer CyCode maintenant ?', 'Recovery stopped. Keep pending.json, transaction backups and recovery files.': 'La récupération s’est arrêtée. Conservez pending.json, les sauvegardes de la transaction et les fichiers de récupération.'})
translations.update({'The previous installation was restored, but CyCode could not restart. Start it manually.':'L’installation précédente a été restaurée, mais CyCode n’a pas pu redémarrer. Lancez-le manuellement.'})
translations.update({'Update storage': 'Stockage des mises à jour', 'Managing update storage...': 'Gestion du stockage des mises à jour…', 'Retiring obsolete downloads is reversible and does not free disk space. Backups, recovery data, current releases and unknown files are protected.': 'L’archivage des anciens téléchargements est réversible et ne libère pas d’espace disque. Les sauvegardes, données de récupération, versions actuelles et fichiers inconnus sont protégés.', 'Inspect update storage': 'Inspecter le stockage des mises à jour', 'Cache operation completed. Inspect again to review the new state.': 'Opération de cache terminée. Relancez l’inspection pour examiner le nouvel état.', 'Cache operation stopped. Retained files and its receipt allow a fresh restoration review.': 'L’opération de cache s’est arrêtée. Les fichiers conservés et leur reçu permettent un nouvel examen de restauration.', 'Files moved': 'Fichiers déplacés', 'Protected directories': 'Dossiers protégés', 'Protected or unrecognized cache entries': 'Entrées de cache protégées ou non reconnues', 'Storage inspection reached its budget. Some entries are omitted.': 'L’inspection du stockage a atteint sa limite. Certaines entrées sont omises.', 'No eligible obsolete downloads or retired caches were found.': 'Aucun ancien téléchargement admissible ni cache archivé n’a été trouvé.', 'Review retirement': 'Examiner l’archivage', 'Files to restore': 'Fichiers à restaurer', 'Conflicts': 'Conflits', 'Review cache restoration': 'Examiner la restauration du cache', 'Selected cache action': 'Action de cache sélectionnée', 'I reviewed this cache action': 'J’ai examiné cette action sur le cache', 'Restore reviewed cache files': 'Restaurer les fichiers de cache examinés', 'Retire reviewed download': 'Archiver le téléchargement examiné'})
translations.update({'Last cache operation':'Dernière opération sur le cache'})
translations.update({'Incomplete update downloads': 'Téléchargements de mise à jour incomplets', 'These observed bytes are not verified or installable. Check GitHub Releases, then download again to resume the exact matching archive when possible.': 'Ces octets observés ne sont ni vérifiés ni installables. Vérifiez les versions GitHub, puis relancez le téléchargement pour reprendre exactement la même archive lorsque possible.', 'Download cancelled. Received bytes are retained for an explicit retry.': 'Téléchargement annulé. Les octets reçus sont conservés pour une nouvelle tentative explicite.', 'Download or resume update': 'Télécharger ou reprendre la mise à jour', 'Download or resume newer update': 'Télécharger ou reprendre la version plus récente', 'bytes': 'octets'})

translations.update({'Start a fresh download': 'Démarrer un téléchargement neuf', 'A fresh download ignores existing checkpoints and starts in a new folder. Retained files are not deleted and can use additional disk space.': 'Un téléchargement neuf ignore les points de reprise existants et démarre dans un nouveau dossier. Les fichiers conservés ne sont pas supprimés et peuvent occuper davantage d’espace disque.', 'Download technical details': 'Détails techniques du téléchargement', 'The local download list could not be refreshed. Refresh it manually.': 'La liste locale des téléchargements n’a pas pu être actualisée. Actualisez-la manuellement.', 'Cache refresh cancelled. The download result is unchanged.': 'Actualisation du cache annulée. Le résultat du téléchargement est conservé.', 'GitHub access is unavailable.': 'L’accès à GitHub est indisponible.', 'Check repository access and the update token environment, or retry after rate limiting.': 'Vérifiez l’accès au dépôt et le jeton de mise à jour dans l’environnement, ou réessayez après la limitation des requêtes.', 'The update connection was interrupted.': 'La connexion de mise à jour a été interrompue.', 'Check the connection, then retry to resume retained bytes when possible.': 'Vérifiez la connexion, puis réessayez pour reprendre les octets conservés lorsque possible.', 'The server response cannot be used safely.': 'La réponse du serveur ne peut pas être utilisée en toute sécurité.', 'Try a fresh download, or open the release for a manual download.': 'Essayez un téléchargement neuf, ou ouvrez la version pour la télécharger manuellement.', 'The update could not be saved or staged.': 'La mise à jour n’a pas pu être enregistrée ou préparée.', 'Check free space, permissions and files in use. Retained downloads are not removed.': 'Vérifiez l’espace libre, les autorisations et les fichiers en cours d’utilisation. Les téléchargements conservés ne sont pas supprimés.', 'The downloaded files failed integrity verification.': 'La vérification d’intégrité des fichiers téléchargés a échoué.', 'Check the release again and start a fresh download. Rejected files remain on disk.': 'Vérifiez à nouveau la version et démarrez un téléchargement neuf. Les fichiers rejetés restent sur le disque.', 'Another instance is using update storage.': 'Une autre instance utilise le stockage des mises à jour.', 'Wait for the other update operation to finish, then retry. No instance is terminated.': 'Attendez la fin de l’autre opération de mise à jour, puis réessayez. Aucune instance n’est arrêtée.', 'This installation cannot accept the update.': 'Cette installation ne peut pas recevoir la mise à jour.', 'Resolve the pending recovery or install a current portable distribution from GitHub Releases.': 'Résolvez la récupération en attente ou installez une distribution portable actuelle depuis les versions GitHub.', 'The update download could not be completed.': 'Le téléchargement de la mise à jour n’a pas pu être terminé.', 'Refresh the release and review the technical details before retrying.': 'Actualisez la version et consultez les détails techniques avant de réessayer.'})

translations.update({'Preparing update download...': 'Préparation du téléchargement de la mise à jour…', 'Verifying update archive...': 'Vérification de l’archive de mise à jour…', 'Preparing update files...': 'Préparation des fichiers de mise à jour…', 'Update download verified': 'Téléchargement de la mise à jour vérifié', 'Update download stopped': 'Téléchargement de la mise à jour arrêté', 'There is not enough space for this update download.': 'L’espace disponible est insuffisant pour télécharger cette mise à jour.', 'Free space on the installation drive, then retry. Retained files are not deleted automatically.': 'Libérez de l’espace sur le disque de l’installation, puis réessayez. Les fichiers conservés ne sont pas supprimés automatiquement.', 'Download target': 'Version en cours de téléchargement', 'MiB': 'Mio', 'MiB/s': 'Mio/s', 'seconds': 'secondes', 'Initial retained prefix': 'Octets conservés au départ', 'Received this attempt': 'Reçu pendant cette tentative', 'Transfer rate': 'Débit du transfert', 'Estimated time remaining': 'Temps restant estimé', 'Estimating remaining time...': 'Estimation du temps restant…', 'Estimated additional space': 'Espace supplémentaire estimé', 'Last observed available space': 'Dernier espace disponible observé', 'Space is checked before transfer, full-download fallback and staging. This estimate is not a reservation or an installation-space guarantee.': 'L’espace est vérifié avant le transfert, le repli vers un téléchargement complet et la préparation. Cette estimation ne réserve pas d’espace et ne garantit pas celui nécessaire à l’installation.', 'The server ignored resume. The earlier partial is retained; a full download needs its own space.': 'Le serveur a ignoré la reprise. Le fichier incomplet précédent est conservé ; un téléchargement complet nécessite son propre espace.'})

translations.update({'Estimated transfer time remaining':'Temps de transfert restant estimé','Last download observation':'Dernière observation du téléchargement','Download target':'Version du téléchargement'})

translations.update({'Update preparation failed before replacing installed files.': 'La préparation de la mise à jour a échoué avant le remplacement des fichiers installés.', 'The update was blocked by available disk space before replacement.': 'L’espace disque disponible a bloqué la mise à jour avant le remplacement des fichiers.', 'The update failed and the previous installation was verified after rollback.': 'La mise à jour a échoué. L’installation précédente a été vérifiée après le retour arrière.', 'The update stopped and requires recovery. Keep pending.json and all backups.': 'La mise à jour s’est arrêtée et nécessite une récupération. Conservez pending.json et toutes les sauvegardes.', 'The application update could not be completed.': 'La mise à jour de l’application n’a pas pu être terminée.', 'CyCode update': 'Mise à jour de CyCode', 'The update was installed, but CyCode could not restart. Start it manually.': 'La mise à jour est installée, mais CyCode n’a pas pu redémarrer. Lancez-le manuellement.'})

translations.update({'Recovery storage could not be inspected. No installation files were changed.': 'Le stockage nécessaire à la récupération n’a pas pu être inspecté. Aucun fichier installé n’a été modifié.', 'Estimated additional recovery space': 'Espace supplémentaire estimé pour la récupération', 'There is not enough space to prepare recovery. No installation files were changed.': 'L’espace disponible est insuffisant pour préparer la récupération. Aucun fichier installé n’a été modifié.', 'Free space on the installation drive, then run this helper again. Keep pending.json and all backups.': 'Libérez de l’espace sur le disque de l’installation, puis relancez cet outil. Conservez pending.json et toutes les sauvegardes.', 'Recovery copies are prepared and verified before installed files move. This space estimate is not a reservation.': 'Les copies de récupération sont préparées et vérifiées avant le déplacement des fichiers installés. Cette estimation ne réserve pas d’espace.'})

translations.update({'Verified prepared copies available': 'Copies préparées et vérifiées disponibles', 'Files to copy from backups': 'Fichiers à copier depuis les sauvegardes', 'Some preparation records were ignored. Existing files are retained and required originals will be copied from backups.': 'Certains enregistrements de préparation ont été ignorés. Les fichiers existants sont conservés et les originaux nécessaires seront copiés depuis les sauvegardes.', 'Prepared copies are verified again before use. Original backups remain required and are retained.': 'Les copies préparées sont revérifiées avant leur utilisation. Les sauvegardes d’origine restent nécessaires et sont conservées.'})

translations.update({'Inspecting recovery files...': 'Inspection des fichiers de récupération…', 'Preparing recovery copies...': 'Préparation des copies de récupération…', 'Verifying prepared recovery files...': 'Vérification des fichiers de récupération préparés…', 'Restoring installed files...': 'Restauration des fichiers installés…', 'Verifying the restored installation...': 'Vérification de l’installation restaurée…', 'Recovery completed': 'Récupération terminée', 'Recovery cancelled': 'Récupération annulée', 'Recovery failed': 'Échec de la récupération', 'Prepared copies': 'Copies préparées', 'Restored entries': 'Éléments restaurés', 'Copied during this attempt': 'Copié pendant cette tentative', 'Stopping after the current operation...': 'Arrêt après l’opération en cours…', 'Stop requests are cooperative. Keep this window open until recovery finishes.': 'La demande d’arrêt sera traitée entre les opérations. Gardez cette fenêtre ouverte jusqu’à la fin de la récupération.', 'Stop recovery': 'Arrêter la récupération', 'The recovery report could not be copied.': 'Le compte rendu de récupération n’a pas pu être copié.', 'This recovery attempt stopped after installed files moved. Keep pending.json and all backups, then run this helper again.': 'Cette tentative de récupération s’est arrêtée après le déplacement de fichiers installés. Conservez pending.json et toutes les sauvegardes, puis relancez cet outil.', 'This recovery attempt was cancelled before installed files moved. Keep pending.json and all backups.': 'Cette tentative de récupération a été annulée avant le déplacement des fichiers installés. Conservez pending.json et toutes les sauvegardes.', 'Technical details': 'Détails techniques', 'Restart CyCode': 'Redémarrer CyCode', 'Copy recovery report': 'Copier le compte rendu de récupération'})

translations.update({
    'Reading update history...': 'Lecture de l’historique des mises à jour…',
    'Recorded recovery completed': 'Restauration terminée selon le reçu',
    'Recorded recovery cancelled': 'Restauration annulée selon le reçu',
    'Recorded recovery failed': 'Échec de restauration selon le reçu',
    'History inspection cancelled. The previous snapshot is unchanged.': 'Inspection de l’historique annulée. Le résultat précédent est conservé.',
    'Inspect detailed update history': 'Inspecter l’historique détaillé des mises à jour',
    'History is a cached metadata observation, not verification of the current installation. Refresh explicitly; no archive or backup payload is read.': 'Cet historique présente les métadonnées observées, sans vérifier l’état actuel de l’installation. Actualisez-le explicitement ; le contenu des archives et sauvegardes n’est pas lu.',
    'Filter versions, results, dates or paths...': 'Filtrer les versions, résultats, dates ou chemins…',
    'All history entries': 'Toutes les entrées de l’historique',
    'Needs attention': 'À examiner',
    'Installations': 'Installations',
    'Recoveries': 'Restaurations',
    'History category': 'Catégorie de l’historique',
    'Date unavailable': 'Date indisponible',
    'Page': 'Page',
    'Previous history page': 'Page précédente de l’historique',
    'Next history page': 'Page suivante de l’historique',
    'History inspection reached its budget. Some records are omitted; pages cover this snapshot only.': 'L’inspection de l’historique a atteint sa limite. Des entrées sont omises ; les pages couvrent uniquement ce résultat.',
    'Invalid history records ignored': 'Entrées d’historique invalides ignorées',
    'No history entries match the filters.': 'Aucune entrée de l’historique ne correspond aux filtres.',
    'Selected history entry': 'Entrée d’historique sélectionnée',
    'This attempt recorded installed-file moves.': 'Cette tentative a enregistré des déplacements de fichiers installés.',
    'This attempt recorded no installed-file moves.': 'Cette tentative n’a enregistré aucun déplacement de fichier installé.',
    'The receipt records completed pending-marker removal.': 'Le reçu indique que le marqueur de récupération a été supprimé.',
    'The receipt does not record pending-marker removal.': 'Le reçu n’indique pas de suppression du marqueur de récupération.',
    'Copy selected history entry': 'Copier l’entrée d’historique sélectionnée'
})
translations.update({'Help': 'Aide', 'Documentation': 'Documentation', 'Context help': 'Aide contextuelle', 'Illustrated tour': 'Visite illustrée', 'Keyboard shortcuts': 'Raccourcis clavier', 'Extension documentation': 'Documentation des extensions', 'MCP documentation': 'Documentation MCP', 'Editor documentation': 'Documentation de l’éditeur'})
translations.update({
    'Insert snippet...': 'Insérer un extrait...',
    'Surround with snippet...': 'Encadrer avec un extrait...',
    'Edit project snippets...': 'Modifier les extraits du projet...',
    'Edit user snippets...': 'Modifier les extraits personnels...',
    'Switch header/source': 'Basculer entre en-tête et source',
    'Loading related C/C++ files...': 'Recherche des fichiers C/C++ associés...',
    'No related C/C++ files found.': 'Aucun fichier C/C++ associé trouvé.',
    'Related-file search was limited or some paths were unreadable.': 'La recherche des fichiers associés est limitée ou certains chemins sont illisibles.',
    'Header/source context changed. Run the command again.': 'Le contexte en-tête/source a changé. Relancez la commande.',
    'Sibling': 'Voisin',
    'Paired directory': 'Dossier lié',
    'Paired root': 'Racine liée',
    'Workspace index': 'Index du projet',
    'Open included file': 'Ouvrir le fichier inclus',
    'Loading included files...': 'Recherche des fichiers inclus...',
    'No matching included file found.': 'Aucun fichier inclus correspondant trouvé.',
    'Source-relative': 'Relatif à la source',
    'For compiler or SDK headers, use Go to definition.': 'Pour les en-têtes du compilateur ou du SDK, utilisez Aller à la définition.',
    'Include navigation context changed. Run the command again.': 'Le contexte de navigation de l’inclusion a changé. Relancez la commande.',
    'No supported literal #include in this directive (1 MiB prefix / 8 KiB directive limits)': 'Aucune inclusion littérale prise en charge dans cette directive (limites : préfixe de 1 Mio et directive de 8 Kio)',
    'Find related C/C++ files...': 'Rechercher les fichiers C/C++ associés...',
    'Related C/C++ files': 'Fichiers C/C++ associés',
    'Retry this lookup': 'Relancer cette recherche',
    'Refresh lookup index and retry': 'Actualiser l’index de recherche et relancer',
    'Copy all result paths': 'Copier tous les chemins des résultats',
    'Copy lookup report (JSON)': 'Copier le rapport de recherche (JSON)',
    'Help for this navigation': 'Aide pour cette navigation',
    'Refreshing lookup index...': 'Actualisation de l’index de recherche...',
    'File index refresh is already in progress. Retry when it is ready.': 'L’actualisation de l’index des fichiers est déjà en cours. Réessayez lorsqu’il sera prêt.',
    'Workspace file index is unavailable. Refresh the project files, then retry.': 'L’index des fichiers du projet est indisponible. Actualisez les fichiers puis réessayez.',
    'Workspace file index is being refreshed. Retry when it is ready.': 'L’index des fichiers du projet est en cours d’actualisation. Réessayez lorsqu’il sera prêt.',
    'Workspace file index changed. Refresh the project files, then retry.': 'L’index des fichiers du projet a changé. Actualisez les fichiers puis réessayez.',
    'Workspace file index is incomplete. Other matching paths may be missing.': 'L’index des fichiers du projet est incomplet. D’autres chemins correspondants peuvent manquer.',
    'Create user snippet from selection...': 'Créer un extrait personnel depuis la sélection...',
    'Selection copied into a new draft. Edit its name, prefix and scope, then Save.': 'Sélection copiée dans un nouveau brouillon. Modifiez son nom, son préfixe et sa portée, puis enregistrez.',
    'Shared by all projects in this portable installation.': 'Partagés par tous les projets de cette installation portable.',
    'User snippet storage is not configured in this host.': 'Le stockage des extraits personnels n’est pas configuré dans cet hôte.',
    'User snippets have unsaved changes.': 'Les extraits personnels contiennent des modifications non enregistrées.',
    'Keep editing': 'Continuer l’édition',
    'Save and close': 'Enregistrer et fermer',
    'Save and quit': 'Enregistrer et quitter',
    'Wait for user snippet storage before closing.': 'Attendez la fin de l’accès aux extraits personnels avant de fermer.',
    'Discard and reload': 'Abandonner et recharger',
    'Discard changes': 'Abandonner les modifications',
    'Reload saved snippets': 'Recharger les extraits enregistrés',
    'This snippet does not use TM_SELECTED_TEXT.': 'Cet extrait n’utilise pas TM_SELECTED_TEXT.',
    'This snippet cannot wrap the current selection.': 'Cet extrait ne peut pas encadrer la sélection courante.',
    'Loading snippets...': 'Chargement des extraits...',
    'No matching snippets. Save definitions in .cycode/snippets.code-snippets.': 'Aucun extrait correspondant. Enregistrez les définitions dans .cycode/snippets.code-snippets.',
    'No matching snippets. Save project definitions or enable a snippet extension.': 'Aucun extrait correspondant. Enregistrez des définitions dans le projet ou activez une extension d’extraits.',
    'Snippet context changed. Request snippets again.': 'Le contexte a changé. Demandez à nouveau les extraits.',
    'Snippet catalogue limit reached; narrow the language scopes.': 'La limite du catalogue est atteinte ; précisez les langages des extraits.'
})

translations.update({
    'Automatic installation currently supports Windows x64 installed and portable packages. Use GitHub Releases for other platforms.': 'L’installation automatique prend en charge les éditions Windows x64 installées et portables. Utilisez GitHub Releases pour les autres plateformes.',
    'This copy is a development checkout or a legacy package. Download a current installed or portable release to enable installation updates.': 'Cette copie est un dépôt de développement ou un ancien paquet. Téléchargez une édition installée ou portable actuelle pour activer les mises à jour intégrées.'
})
translations.update({
    'Installed package files changed; review them before updating': 'Des fichiers du programme installé ont changé ; examinez-les avant la mise à jour',
    'Update package differs from the installation edition': 'Le paquet de mise à jour ne correspond pas à l’édition installée',
    'Installed update runtime changed': 'Le runtime installé de mise à jour a changé',
    'Staged update runtime changed': 'Le runtime préparé pour la mise à jour a changé',
    'Cannot start the verified Windows installer': 'Impossible de lancer l’installateur Windows vérifié',
    'Windows installation was cancelled or failed': 'L’installation Windows a été annulée ou a échoué',
    'Windows installation did not produce the expected version': 'L’installation Windows n’a pas produit la version attendue'
})
translations.update({
    'Text size must be between 0.8 and 2.0': 'La taille du texte doit être comprise entre 0,8 et 2,0',
    'Select a clangd executable': 'Sélectionnez un exécutable clangd',
    'Build timeout must be an integer from 1 to 86400 seconds': 'La durée limite de compilation doit être un entier compris entre 1 et 86 400 secondes',
    'Build timeout must be positive': 'La durée limite de compilation doit être positive'
})
translations.update({
    'Reset diagnostic filters': 'Réinitialiser les filtres des diagnostics',
    'No matching diagnostics': 'Aucun diagnostic correspondant',
    'Change the text or severity filters, or reset them to show all diagnostics.': 'Modifiez les filtres de texte ou de gravité, ou réinitialisez-les pour afficher tous les diagnostics.',
    'No matching code actions': 'Aucune action contextuelle correspondante',
    'Change or clear the filter to show context actions.': 'Modifiez ou effacez le filtre pour afficher les actions contextuelles.'
})
translations.update({
    'Ctrl + mouse wheel zooms code in this view. Click to reset to 100%.': 'Ctrl + molette zoome le code dans cette vue. Cliquez pour revenir à 100 %.'
})
translations.update({'Analysis and inspections': 'Analyse et inspections', 'Analysis progress': 'Progression de l’analyse', 'C++ inspections': 'Inspections C++', 'Ready for the next batch': 'Prêt pour le lot suivant', 'Service and language backends': 'Service et serveurs de langage', 'Native Engine attached': 'Moteur natif partagé connecté', 'Native Engine unavailable': 'Moteur natif indisponible', 'Exact shared-service lifecycle status is unavailable through this Core source API.': 'L’état exact du cycle de vie du service partagé n’est pas exposé par cette API source Core.', 'File inventory': 'Inventaire des fichiers', 'Inventory currently available': 'Inventaire actuellement disponible', 'Waiting for language server': 'En attente du serveur de langage', 'Active file diagnostics': 'Diagnostics du fichier actif', 'Current revision': 'Révision actuelle', 'Not observed at the current revision': 'Non observé sur la révision actuelle', 'Inventory, active-file analysis and whole-project semantic coverage are different. Index completeness remains unknown.': 'Inventaire, analyse du fichier actif et couverture sémantique globale sont distincts. La complétude de l’index reste inconnue.', 'Active C++ file': 'Fichier C++ actif', 'Path prefix': 'Préfixe de chemin', 'Persist traversal checkpoint': 'Conserver le point de reprise du parcours', 'Sources per batch': 'Sources par lot', 'Zero inventories without compiling. Each next batch requires an explicit request.': 'Zéro inventorie sans compiler. Chaque lot suivant exige une demande explicite.', 'Prepare selected scope': 'Préparer la portée choisie', 'Resume next batch': 'Reprendre le lot suivant', 'Pause analysis': 'Mettre l’analyse en pause', 'Source traversal': 'Parcours des sources', 'Historical observations': 'Observations historiques', 'Current-pass observations': 'Observations de la passe courante', 'Restored observations are historical and unknown for this pass. They do not authorize current edits.': 'Les observations restaurées sont historiques et inconnues pour cette passe. Elles n’autorisent aucune modification actuelle.', 'Refresh result page': 'Actualiser la page de résultats', 'Historical observation': 'Observation historique', 'Current-pass observation': 'Observation de la passe courante', 'Discover analysis checkpoints': 'Découvrir les points de reprise d’analyse', 'Restore traversal': 'Restaurer le parcours', 'Checkpoint discovery was limited or contains invalid records. No checkpoint was deleted.': 'La découverte est limitée ou contient des entrées invalides. Aucun point de reprise n’a été supprimé.', 'Inspect one saved C++ translation unit with its real compilation command. No fixes are applied or sources saved.': 'Inspecter une unité C++ enregistrée avec sa véritable commande de compilation. Aucun correctif ni enregistrement des sources n’est effectué.', 'Open a saved C++ source and resolve unsaved changes or disk conflicts before inspection.': 'Ouvrez une source C++ enregistrée et résolvez les modifications non enregistrées ou conflits disque avant l’inspection.', 'Inspection profile': 'Profil d’inspection', 'clang-tidy executable': 'Exécutable clang-tidy', 'Leave empty to use Core tool discovery. A missing tool is reported explicitly.': 'Laisser vide pour utiliser la détection Core. Un outil manquant est signalé explicitement.', 'Inspect saved source': 'Inspecter la source enregistrée', 'Cancel inspection': 'Annuler l’inspection', 'Native process running': 'Processus natif en cours', 'Waiting for native process': 'En attente du processus natif', 'Inspection results are stale. Inspect the current saved revision again.': 'Les résultats sont périmés. Inspectez à nouveau la révision actuelle enregistrée.', 'Inspection output or diagnostics were truncated.': 'La sortie ou les diagnostics d’inspection ont été tronqués.', 'Compiler locations use one-based UTF-8 byte columns, not LSP UTF-16 positions.': 'Les emplacements du compilateur utilisent des colonnes d’octets UTF-8 depuis un, distinctes des positions LSP UTF-16.', 'Open location': 'Ouvrir l’emplacement', 'Copy inspection output': 'Copier la sortie d’inspection', 'Request a fresh backend result. Index progress may leave cached results.': 'Demander un nouveau résultat au serveur. La progression de l’index peut laisser des résultats en cache.'})
translations.update({'Partial byte preview. Apply and Undo affect the complete native change plan.': 'Aperçu partiel en octets. Appliquer et annuler concernent le plan natif complet.', 'First preview page': 'Première page de l’aperçu', 'Next preview page': 'Page suivante de l’aperçu'})
translations.update({'Checkpoint': 'Point de reprise', 'Compilation database': 'Base de commandes de compilation', 'Open': 'Ouvrir', 'Scope': 'Portée', 'Shared native service': 'Service natif partagé', 'Service details': 'Détails du service', 'Shared Engine context': 'Contexte du moteur partagé', 'Core source version': 'Version des sources Core', 'Connect agents to this Engine': 'Connecter les agents à ce moteur', 'Status pending': 'État en attente', 'Clean diagnostics': 'Diagnostics sans erreur', 'Diagnostics with errors': 'Diagnostics avec erreurs', 'Unknown diagnostics': 'Diagnostics inconnus', 'Unvisited sources': 'Sources non visitées', 'Elapsed (ms)': 'Durée (ms)', 'Recommended': 'Recommandé', 'Security': 'Sécurité', 'Modernize': 'Modernisation', 'Performance': 'Performance', 'Portability': 'Portabilité', 'Use configured compilation database': 'Utiliser la base de compilation configurée', 'Inspection documentation': 'Documentation des inspections', 'No diagnostics in this result.': 'Aucun diagnostic dans ce résultat.'})
for en, fr in {
    "Stopping": "Arrêt en cours",
    "Complete": "Terminé",
    "Truncated": "Tronqué",
    "Stale results": "Résultats périmés",
    "Timeout (seconds)": "Délai maximal (secondes)",
    "Diagnostics outside the workspace": "Diagnostics hors de l’espace de travail",
}.items():
    translations[en] = fr

for key, value in {'Open with paged text': 'Ouvrir en texte paginé', 'Acknowledge pending input': 'Acquitter les saisies en attente', 'Checkpoint active paged text': 'Créer un point de reprise du texte paginé actif', 'Checkpoint baseline differs from disk. Reconcile the source before restoration.': 'La base du point de reprise diffère du disque. Réconciliez la source avant la restauration.', 'Copy pending input': 'Copier les saisies en attente', 'Discarded native history entries': 'Entrées sorties de l’historique natif', 'Discover paged checkpoints': 'Découvrir les points de reprise paginés', 'Explicit paged recovery': 'Récupération paginée explicite', 'Find in paged text': 'Rechercher dans le texte paginé', 'Finish pending input and native operations before saving.': 'Terminez les saisies en attente et les opérations natives avant d’enregistrer.', 'Native change history': 'Historique des modifications natives', 'Native paged document was closed externally. Recover pending input before closing this view.': 'Le document paginé natif a été fermé par un autre client. Récupérez les saisies en attente avant de fermer cette vue.', 'Native transactions are shared with attached agents. Prepare and review the complete change before applying.': 'Les transactions natives sont partagées avec les agents connectés. Préparez et examinez la modification complète avant de l’appliquer.', 'Next': 'Suivant', 'Open paged source': 'Ouvrir la source paginée', 'Opening native paged text...': 'Ouverture du texte paginé natif…', 'Ordinary-buffer checkpoints: every second. Paged checkpoints and source saves are explicit.': 'Points de reprise des tampons ordinaires : chaque seconde. Les points paginés et l’enregistrement des sources sont explicites.', 'Paged UTF-8': 'UTF-8 paginé', 'Paged UTF-8. No LSP overlay; save or close unsaved paged edits before compiler queries.': 'UTF-8 paginé, sans superposition LSP : enregistrez ou fermez les modifications paginées avant une requête au compilateur.', 'Paged checkpoints': 'Points de reprise paginés', 'Paged checkpoints are explicit. Publishing one does not save the source; restore is a reviewed native change with Undo.': 'Les points de reprise paginés sont explicites. Leur publication n’enregistre pas la source ; la restauration est une modification native examinée et annulable.', 'Pending input was cancelled. Recover it before closing or saving.': 'Des saisies en attente ont été annulées. Récupérez-les avant de fermer ou d’enregistrer.', 'Prepare checkpoint restoration': 'Préparer la restauration du point de reprise', 'Prepare native Undo': 'Préparer l’annulation native', 'Prepare native Redo': 'Préparer le rétablissement natif', 'Preview': 'Aperçu', 'Previous': 'Précédent', 'Refresh native history': 'Actualiser l’historique natif', 'Refresh paged state': 'Actualiser l’état paginé', 'Replace current match': 'Remplacer l’occurrence actuelle', 'Save or close modified documents before changing text profiles.': 'Enregistrez ou fermez les documents modifiés avant de changer de profil de texte.', 'Save paged text and wait for native operations before building.': 'Enregistrez le texte paginé et attendez la fin des opérations natives avant de compiler.', 'Wait for the current paged operation.': 'Attendez la fin de l’opération paginée en cours.', 'Whole words': 'Mots entiers', 'Loading native paged backend...': 'Chargement du backend paginé natif…', 'Native operation / index in progress...': 'Opération native ou indexation en cours…', 'Syntax unavailable above 32 MiB: shared streaming lexer required.': 'Coloration indisponible au-delà de 32 Mio : analyseur lexical partagé en flux requis.'}.items():
    translations.setdefault(key, value)

for key, value in {'Cannot prepare UTF-8 line fragment': 'Impossible de préparer le fragment de ligne UTF-8', 'Clipboard publication failed; selection was not cut.': 'La copie vers le presse-papiers a échoué ; la sélection n’a pas été coupée.', 'Clipboard result discarded after a revision change.': 'Résultat du presse-papiers abandonné après un changement de révision.', 'Clipboard selection exceeds the 4 MiB export budget.': 'La sélection dépasse le budget d’export de 4 Mio du presse-papiers.', 'Committed edit; view acknowledgement was cancelled.': 'Modification validée ; l’acquittement de la vue a été annulé.', 'Importing and indexing UTF-8 text...': 'Import et indexation du texte UTF-8…', 'Insertion exceeds the 1 MiB operation budget': 'L’insertion dépasse le budget de 1 Mio par opération', 'Invalid native UTF-8 byte continuation': 'Continuation native en octets UTF-8 invalide', 'Invalid native paged backend state': 'État invalide du backend natif paginé', 'Invalid native paged search range': 'Plage native de recherche paginée invalide', 'Invalid native prepared edit identity': 'Identité de modification native préparée invalide', 'Invalid paged read budget': 'Budget de lecture paginée invalide', 'Native Undo did not issue a committed revision': 'L’annulation native n’a pas fourni de nouvelle révision', 'Native apply did not issue a committed revision': 'La validation native n’a pas fourni de nouvelle révision', 'Native search exceeds its result budget': 'La recherche native dépasse son budget de résultats', 'No native transaction available.': 'Aucune transaction native disponible.', 'Page preparation exceeds its byte/token budget': 'La préparation de page dépasse son budget d’octets ou de jetons', 'Paged document is read-only': 'Le document paginé est en lecture seule', 'Paged read cancelled': 'Lecture paginée annulée', 'Paged revision counter exhausted': 'Compteur de révisions paginées épuisé', 'Recover cancelled input before editing or saving.': 'Récupérez les saisies annulées avant de modifier ou d’enregistrer.', 'Saving the complete native document...': 'Enregistrement du document natif complet…', 'Search must include the complete CRLF pair.': 'La recherche doit inclure la paire CRLF complète.', 'Search query must contain 1 to 4096 bytes': 'La recherche doit contenir de 1 à 4096 octets', 'Stale paged document revision': 'Révision périmée du document paginé', 'Standalone durable paged recovery is unavailable.': 'La récupération durable du texte paginé autonome est indisponible.', 'Syntax highlighting is unavailable above 32 MiB.': 'La coloration est indisponible au-delà de 32 Mio.', 'Wrong paged document identity': 'Identité incorrecte du document paginé'}.items():
    translations.setdefault(key, value)

for key, value in {'Paged text documentation': 'Documentation du texte paginé', 'Save As publishes the complete paged document to a new file and resets native history for the old path.': 'Enregistrer sous publie le document paginé complet dans un nouveau fichier et réinitialise l’historique natif lié à l’ancien chemin.', 'Review multi-file native changes in Refactoring before Undo or Redo.': 'Examinez les modifications natives multifichiers dans Refactorisation avant de les annuler ou de les rétablir.', 'Review annotated native changes in Refactoring before Redo.': 'Examinez les modifications natives annotées dans Refactorisation avant de les rétablir.', 'Committed. Refresh the native paged state before editing.': 'Modification validée. Actualisez l’état paginé natif avant de modifier le texte.', 'Native edit committed; some notifications failed. Refresh the native paged state.': 'Modification native validée ; certaines notifications ont échoué. Actualisez l’état paginé natif.', 'Native paged document was replaced or closed': 'Le document paginé natif a été remplacé ou fermé', 'Native paged document was closed': 'Le document paginé natif a été fermé', 'Native transaction committed without its paged document receipt; do not retry': 'Transaction native validée sans reçu du document paginé ; ne relancez pas l’opération'}.items():
    translations.setdefault(key, value)

translations.setdefault("Core runtime version", "Version du moteur Core")

for key, value in {'CyCodeIDE update recovery': 'Récupération de mise à jour de CyCodeIDE', 'No interrupted installation is pending. Open CyCodeIDE to install downloaded updates.': 'Aucune installation interrompue n’est en attente. Ouvrez CyCodeIDE pour installer les mises à jour téléchargées.', 'The previous installation was restored. Original backups were retained. Restart CyCodeIDE now?': 'L’installation précédente a été restaurée. Les sauvegardes d’origine ont été conservées. Redémarrer CyCodeIDE maintenant ?', 'CyCodeIDE update': 'Mise à jour de CyCodeIDE', 'The update was installed, but CyCodeIDE could not restart. Start it manually.': 'La mise à jour est installée, mais CyCodeIDE n’a pas pu redémarrer. Lancez-le manuellement.', 'Restart CyCodeIDE': 'Redémarrer CyCodeIDE'}.items():
    translations.setdefault(key, value)

for key, value in {'Private releases use CYCODEIDE_UPDATE_TOKEN, GH_TOKEN or GITHUB_TOKEN from the environment. Credentials are never saved in preferences.': 'Les versions privées utilisent CYCODEIDE_UPDATE_TOKEN, GH_TOKEN ou GITHUB_TOKEN depuis l’environnement. Les identifiants ne sont jamais enregistrés dans les préférences.'}.items():
    translations.setdefault(key, value)

for key, value in {'Restore the previous installation? Projects and user data are preserved. Close all CyCodeIDE instances before continuing.': 'Restaurer l’installation précédente ? Les projets et données utilisateur sont conservés. Fermez toutes les instances de CyCodeIDE avant de continuer.', 'The previous installation was restored, but CyCodeIDE could not restart. Start it manually.': 'L’installation précédente a été restaurée, mais CyCodeIDE n’a pas pu redémarrer. Lancez-le manuellement.', 'To recover an interrupted update, run the staged CyCodeIDEUpdater.exe inside data/updates/download-... . Keep pending.json and all backups.': 'Pour récupérer une mise à jour interrompue, lancez le CyCodeIDEUpdater.exe conservé dans data/updates/download-... . Conservez pending.json et toutes les sauvegardes.'}.items():
    translations.setdefault(key, value)

path.write_text(json.dumps({key: {'en': key, 'fr': value} for key,value in translations.items()}, indent=2, ensure_ascii=False)+'\n', encoding='utf-8')
