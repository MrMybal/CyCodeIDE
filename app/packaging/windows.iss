; CyCodeIDE's per-user identity is distinct from historical CyCode and Reader.
#if !defined(UpdateProduct) || !defined(UpdateRepository) || !defined(InstallMarker)
  #error Use the pinned Core installer author with an explicit consumer identity
#endif
#if UpdateProduct != "CyCodeIDE" || UpdateRepository != "MrMybal/CyCodeIDE" || InstallMarker != "CyCodeIDE-installed.flag"
  #error Wrong update product, repository or installed marker
#endif
#ifndef AppVersion
  #error AppVersion is required
#endif
#ifndef SourceDir
  #error SourceDir is required
#endif
#ifndef OutputDir
  #error OutputDir is required
#endif
[Setup]
AppId={{5444F092-A643-4F0B-A7AD-E98B88657AB9}
AppName=CyCodeIDE
AppVersion={#AppVersion}
AppPublisher=Cyberalien
AppPublisherURL=https://github.com/MrMybal/CyCodeIDE
AppSupportURL=https://github.com/MrMybal/CyCodeIDE/issues
AppUpdatesURL=https://github.com/MrMybal/CyCodeIDE/releases
DefaultDirName={localappdata}\Programs\CyCodeIDE
DefaultGroupName=CyCodeIDE
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#OutputDir}
OutputBaseFilename=CyCodeIDE-{#AppVersion}-windows-x64-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
LicenseFile={#SourceDir}\LICENSE
SetupIconFile={#SourceDir}\icons\CyCode.ico
WizardSmallImageFile={#SourceDir}\icons\CyCode.png
UninstallDisplayIcon={app}\CyCodeIDE.exe
CloseApplications=no
RestartApplications=no
SetupLogging=yes
DisableProgramGroupPage=yes
[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"
[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
[Icons]
Name: "{autoprograms}\CyCodeIDE"; Filename: "{app}\CyCodeIDE.exe"
Name: "{autodesktop}\CyCodeIDE"; Filename: "{app}\CyCodeIDE.exe"; Tasks: desktopicon
[CustomMessages]
english.CloseInstances=Close all CyCodeIDE instances using this folder, then retry.
french.CloseInstances=Fermez toutes les instances de CyCodeIDE utilisant ce dossier, puis réessayez.
english.RecoveryRequired=An interrupted update requires recovery before installation or removal. Keep the existing installation and use its update recovery tools.
french.RecoveryRequired=Une mise à jour interrompue nécessite une récupération avant installation ou désinstallation. Conservez cette installation et utilisez ses outils de récupération.
english.DevelopmentDirectory=Choose a separate installation folder outside a Git checkout.
french.DevelopmentDirectory=Choisissez un dossier d'installation distinct, hors d'un dépôt Git.
english.ForeignProduct=Choose a separate CyCodeIDE folder. This folder belongs to another product.
french.ForeignProduct=Choisissez un dossier CyCodeIDE distinct. Ce dossier appartient à un autre produit.
[Code]
var InstallationHandle: THandle;
function OpenLease(Name: String; Access, Sharing, Security, Creation, Flags, Template: LongWord): THandle;
  external 'CreateFileW@kernel32.dll stdcall';
function CloseLease(Handle: THandle): Boolean;
  external 'CloseHandle@kernel32.dll stdcall';
function AcquireInstallation(): String;
begin
  Result := '';
  if (InstallationHandle <> 0) and (InstallationHandle <> -1) then exit;
  if DirExists(ExpandConstant('{app}\.git')) or FileExists(ExpandConstant('{app}\.git')) then begin
    Result := CustomMessage('DevelopmentDirectory'); exit;
  end;
  if FileExists(ExpandConstant('{app}\CyCode.exe')) or
     FileExists(ExpandConstant('{app}\CyCodeReader.exe')) or
     FileExists(ExpandConstant('{app}\installed.flag')) or
     FileExists(ExpandConstant('{app}\CyCode-installed.flag')) or
     FileExists(ExpandConstant('{app}\CyCodeReader-installed.flag')) or
     FileExists(ExpandConstant('{app}\portable.flag')) then begin
    Result := CustomMessage('ForeignProduct'); exit;
  end;
  if FileExists(ExpandConstant('{app}\data\updates\pending.json')) then begin
    Result := CustomMessage('RecoveryRequired'); exit;
  end;
  if not ForceDirectories(ExpandConstant('{app}\data\updates')) then begin
    Result := CustomMessage('CloseInstances'); exit;
  end;
  { Hold the same installation ownership file exclusively through Setup/uninstall.
    CyCodeIDE's native shared leases cannot open it until this handle is closed. }
  InstallationHandle := OpenLease(ExpandConstant('{app}\data\updates\instances.lock'), $C0000000, 0, 0, 4, $00200080, 0);
  if InstallationHandle = -1 then Result := CustomMessage('CloseInstances');
end;
procedure ReleaseInstallation();
begin
  if (InstallationHandle <> 0) and (InstallationHandle <> -1) then CloseLease(InstallationHandle);
  InstallationHandle := 0;
end;
function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := AcquireInstallation();
end;
procedure DeinitializeSetup();
begin
  ReleaseInstallation();
end;
function InitializeUninstall(): Boolean;
var Reason: String;
begin
  Reason := AcquireInstallation(); Result := Reason = '';
  if not Result then MsgBox(Reason, mbError, MB_OK);
end;
procedure DeinitializeUninstall();
begin
  ReleaseInstallation();
end;
