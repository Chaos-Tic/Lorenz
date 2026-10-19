; Lorenz installer for Windows (Inno Setup 6). CI builds it with:
;   iscc /DAppVersion=1.1.0 packaging\windows\lorenz.iss
; dist\Lorenz (SourceDir) holds lorenz.exe, lorenz-desktop.exe and the Qt files from windeployqt.
; Silent install for scripts: Lorenz-Setup-x64.exe /VERYSILENT /TASKS="engines,autostart"
#ifndef AppVersion
  #define AppVersion "1.1.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\dist\Lorenz"
#endif

[Setup]
; Never change AppId: Windows recognises updates and the uninstaller by it.
AppId={{3EA054E1-2E93-4DDD-8519-122020F8DE05}
AppName=Lorenz
AppVersion={#AppVersion}
AppPublisher=Chaos-Tic
AppPublisherURL=https://github.com/Chaos-Tic/Lorenz
AppSupportURL=https://github.com/Chaos-Tic/Lorenz/issues
; Per-user, like the earlier script installer: same folder, no administrator rights.
PrivilegesRequired=lowest
DefaultDirName={localappdata}\Programs\Lorenz
DisableDirPage=yes
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
OutputDir=..\..\dist
OutputBaseFilename=Lorenz-Setup-x64
SetupIconFile=..\..\assets\lorenz.ico
UninstallDisplayIcon={app}\lorenz-desktop.exe
UninstallDisplayName=Lorenz
WizardStyle=modern
Compression=lzma2/max
SolidCompression=yes
; The tray companion keeps lorenz-desktop.exe open; close it to replace it.
CloseApplications=force
RestartApplications=no
; Offer French or English, preselected from the Windows display language.
ShowLanguageDialog=yes
LanguageDetectionMethod=uilanguage

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
fr.TaskAutostart=Lancer Lorenz à l'ouverture de session
en.TaskAutostart=Start Lorenz when I sign in
fr.InstallingEngine=Installation de %1...
en.InstallingEngine=Installing %1...
fr.ShortcutComment=Convertir des fichiers localement
en.ShortcutComment=Convert files locally
fr.LaunchLorenz=Lancer Lorenz
en.LaunchLorenz=Start Lorenz
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
; Qt is bundled under the LGPL; THIRD-PARTY-NOTICES.txt (in SourceDir) lists it
; and the crates linked into lorenz.exe (packaging/licenses/notices.sh).
Source: "..\licenses\LGPL-3.0.txt"; DestDir: "{app}"; Flags: ignoreversion

[INI]
; The language picked here is the app's language too ("Language" in lorenz.conf;
; "auto" there would follow the system instead).
Filename: "{localappdata}\lorenz\lorenz.conf"; Section: "General"; Key: "Language"; String: "{language}"

[Icons]
; Start menu and Send to: right-click files > Send to > Lorenz opens the wheels.
Name: "{userprograms}\Lorenz"; Filename: "{app}\lorenz-desktop.exe"; WorkingDir: "{app}"
Name: "{usersendto}\Lorenz"; Filename: "{app}\lorenz-desktop.exe"; WorkingDir: "{app}"
; The desktop icon opens a window (the batch manager): a tray-only start would
; look like nothing happened.
Name: "{userdesktop}\Lorenz"; Filename: "{app}\lorenz-desktop.exe"; Parameters: "--manager"; WorkingDir: "{app}"; Comment: "{cm:ShortcutComment}"; Tasks: desktopicon

[Run]
; Engines one at a time, so a package missing from winget never stops the rest.
; winget skips a package that is already installed.
Filename: "{cmd}"; Parameters: "/c winget install --id ImageMagick.ImageMagick --exact --silent --accept-package-agreements --accept-source-agreements --disable-interactivity"; StatusMsg: "{cm:InstallingEngine,ImageMagick}"; Flags: runhidden; Tasks: engines; Check: HasWinget
Filename: "{cmd}"; Parameters: "/c winget install --id Gyan.FFmpeg --exact --silent --accept-package-agreements --accept-source-agreements --disable-interactivity"; StatusMsg: "{cm:InstallingEngine,FFmpeg}"; Flags: runhidden; Tasks: engines; Check: HasWinget
Filename: "{cmd}"; Parameters: "/c winget install --id JohnMacFarlane.Pandoc --exact --silent --accept-package-agreements --accept-source-agreements --disable-interactivity"; StatusMsg: "{cm:InstallingEngine,Pandoc}"; Flags: runhidden; Tasks: engines; Check: HasWinget
Filename: "{cmd}"; Parameters: "/c winget install --id oschwartz10612.Poppler --exact --silent --accept-package-agreements --accept-source-agreements --disable-interactivity"; StatusMsg: "{cm:InstallingEngine,Poppler}"; Flags: runhidden; Tasks: engines; Check: HasWinget
Filename: "{cmd}"; Parameters: "/c winget install --id QPDF.QPDF --exact --silent --accept-package-agreements --accept-source-agreements --disable-interactivity"; StatusMsg: "{cm:InstallingEngine,qpdf}"; Flags: runhidden; Tasks: engines; Check: HasWinget
Filename: "{cmd}"; Parameters: "/c winget install --id TheDocumentFoundation.LibreOffice --exact --silent --accept-package-agreements --accept-source-agreements --disable-interactivity"; StatusMsg: "{cm:InstallingEngine,LibreOffice}"; Flags: runhidden; Tasks: office; Check: HasWinget
Filename: "{app}\lorenz.exe"; Parameters: "autostart enable"; Flags: runhidden; Tasks: autostart
Filename: "{app}\lorenz.exe"; Parameters: "autostart disable"; Flags: runhidden; Tasks: not autostart
; Started through Explorer so it sees the PATH the engine installers just updated,
; not the one this setup inherited.
Filename: "{win}\explorer.exe"; Parameters: """{app}\lorenz-desktop.exe"""; Description: "{cm:LaunchLorenz}"; Flags: postinstall nowait skipifsilent

[UninstallRun]
Filename: "{app}\lorenz.exe"; Parameters: "autostart disable"; Flags: runhidden; RunOnceId: "DisableAutostart"
Filename: "{sys}\taskkill.exe"; Parameters: "/F /IM lorenz-desktop.exe"; Flags: runhidden; RunOnceId: "StopCompanion"

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
