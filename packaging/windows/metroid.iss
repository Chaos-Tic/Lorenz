; Metroid installer for Windows (Inno Setup 6). CI builds it with:
;   iscc /DAppVersion=0.1.0 packaging\windows\metroid.iss
; dist\Metroid (SourceDir) holds metroid.exe, metroid-desktop.exe and the Qt files from windeployqt.
; Silent install for scripts: Metroid-Setup-x64.exe /VERYSILENT /TASKS="engines,autostart"
#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\dist\Metroid"
#endif

[Setup]
; Never change AppId: Windows recognises updates and the uninstaller by it.
AppId={{2930C43E-A3B6-4184-98E4-C2F9AF502F7D}
AppName=Metroid
AppVersion={#AppVersion}
AppPublisher=Chaos-Tic
AppPublisherURL=https://github.com/Chaos-Tic/Metroid
AppSupportURL=https://github.com/Chaos-Tic/Metroid/issues
; Per-user, like the earlier script installer: same folder, no administrator rights.
PrivilegesRequired=lowest
DefaultDirName={localappdata}\Programs\Metroid
DisableDirPage=yes
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
OutputDir=..\..\dist
OutputBaseFilename=Metroid-Setup-x64
SetupIconFile=..\..\assets\metroid.ico
UninstallDisplayIcon={app}\metroid-desktop.exe
UninstallDisplayName=Metroid
WizardStyle=modern
Compression=lzma2/max
SolidCompression=yes
; The tray companion keeps metroid-desktop.exe open; close it to replace it.
CloseApplications=force
RestartApplications=no

[Languages]
Name: "fr"; MessagesFile: "compiler:Languages\French.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
fr.GroupShortcuts=Raccourcis :
en.GroupShortcuts=Shortcuts:
fr.TaskDesktop=Créer un raccourci sur le Bureau
en.TaskDesktop=Create a desktop shortcut
fr.GroupEngines=Moteurs de conversion (téléchargés avec winget) :
en.GroupEngines=Conversion engines (downloaded with winget):
fr.TaskEngines=ImageMagick, FFmpeg, Pandoc, Poppler et qpdf (recommandé)
en.TaskEngines=ImageMagick, FFmpeg, Pandoc, Poppler and qpdf (recommended)
fr.TaskOffice=LibreOffice pour Word, PowerPoint et les tableurs (environ 350 Mo)
en.TaskOffice=LibreOffice for Word, PowerPoint and spreadsheets (about 350 MB)
fr.GroupStartup=Démarrage :
en.GroupStartup=Startup:
fr.TaskAutostart=Lancer Metroid à l'ouverture de session
en.TaskAutostart=Start Metroid when I sign in
fr.InstallingEngine=Installation de %1...
en.InstallingEngine=Installing %1...
fr.ShortcutComment=Convertir des fichiers localement
en.ShortcutComment=Convert files locally
fr.LaunchMetroid=Lancer Metroid
en.LaunchMetroid=Start Metroid
fr.NoWinget=winget est absent : les moteurs de conversion ne seront pas installés. Installez « App Installer » depuis le Microsoft Store, puis relancez ce programme d'installation.
en.NoWinget=winget is missing, so conversion engines will not be installed. Install "App Installer" from the Microsoft Store, then run this setup again.

[Tasks]
Name: "desktopicon"; Description: "{cm:TaskDesktop}"; GroupDescription: "{cm:GroupShortcuts}"
Name: "engines"; Description: "{cm:TaskEngines}"; GroupDescription: "{cm:GroupEngines}"
Name: "office"; Description: "{cm:TaskOffice}"; GroupDescription: "{cm:GroupEngines}"; Flags: unchecked
Name: "autostart"; Description: "{cm:TaskAutostart}"; GroupDescription: "{cm:GroupStartup}"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion

[Icons]
; Start menu and Send to: right-click files > Send to > Metroid opens the wheels.
Name: "{userprograms}\Metroid"; Filename: "{app}\metroid-desktop.exe"; WorkingDir: "{app}"
Name: "{usersendto}\Metroid"; Filename: "{app}\metroid-desktop.exe"; WorkingDir: "{app}"
; The desktop icon opens a window (the batch manager): a tray-only start would
; look like nothing happened.
Name: "{userdesktop}\Metroid"; Filename: "{app}\metroid-desktop.exe"; Parameters: "--manager"; WorkingDir: "{app}"; Comment: "{cm:ShortcutComment}"; Tasks: desktopicon

[Run]
; Engines one at a time, so a package missing from winget never stops the rest.
; winget skips a package that is already installed.
Filename: "{cmd}"; Parameters: "/c winget install --id ImageMagick.ImageMagick --exact --silent --accept-package-agreements --accept-source-agreements --disable-interactivity"; StatusMsg: "{cm:InstallingEngine,ImageMagick}"; Flags: runhidden; Tasks: engines; Check: HasWinget
Filename: "{cmd}"; Parameters: "/c winget install --id Gyan.FFmpeg --exact --silent --accept-package-agreements --accept-source-agreements --disable-interactivity"; StatusMsg: "{cm:InstallingEngine,FFmpeg}"; Flags: runhidden; Tasks: engines; Check: HasWinget
Filename: "{cmd}"; Parameters: "/c winget install --id JohnMacFarlane.Pandoc --exact --silent --accept-package-agreements --accept-source-agreements --disable-interactivity"; StatusMsg: "{cm:InstallingEngine,Pandoc}"; Flags: runhidden; Tasks: engines; Check: HasWinget
Filename: "{cmd}"; Parameters: "/c winget install --id oschwartz10612.Poppler --exact --silent --accept-package-agreements --accept-source-agreements --disable-interactivity"; StatusMsg: "{cm:InstallingEngine,Poppler}"; Flags: runhidden; Tasks: engines; Check: HasWinget
Filename: "{cmd}"; Parameters: "/c winget install --id QPDF.QPDF --exact --silent --accept-package-agreements --accept-source-agreements --disable-interactivity"; StatusMsg: "{cm:InstallingEngine,qpdf}"; Flags: runhidden; Tasks: engines; Check: HasWinget
Filename: "{cmd}"; Parameters: "/c winget install --id TheDocumentFoundation.LibreOffice --exact --silent --accept-package-agreements --accept-source-agreements --disable-interactivity"; StatusMsg: "{cm:InstallingEngine,LibreOffice}"; Flags: runhidden; Tasks: office; Check: HasWinget
Filename: "{app}\metroid.exe"; Parameters: "autostart enable"; Flags: runhidden; Tasks: autostart
Filename: "{app}\metroid.exe"; Parameters: "autostart disable"; Flags: runhidden; Tasks: not autostart
; Started through Explorer so it sees the PATH the engine installers just updated,
; not the one this setup inherited.
Filename: "{win}\explorer.exe"; Parameters: """{app}\metroid-desktop.exe"""; Description: "{cm:LaunchMetroid}"; Flags: postinstall nowait skipifsilent

[UninstallRun]
Filename: "{app}\metroid.exe"; Parameters: "autostart disable"; Flags: runhidden; RunOnceId: "DisableAutostart"
Filename: "{sys}\taskkill.exe"; Parameters: "/F /IM metroid-desktop.exe"; Flags: runhidden; RunOnceId: "StopCompanion"

[Code]
function HasWinget: Boolean;
begin
  Result := FileExists(ExpandConstant('{localappdata}\Microsoft\WindowsApps\winget.exe'));
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if (CurPageID = wpReady) and (WizardIsTaskSelected('engines') or WizardIsTaskSelected('office'))
    and not HasWinget then
    MsgBox(CustomMessage('NoWinget'), mbInformation, MB_OK);
end;
