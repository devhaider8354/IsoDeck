; IsoDeck installer (Inno Setup 6). Built by build.bat with /DBuildDir=<dir containing IsoDeck.exe>
#ifndef BuildDir
  #define BuildDir "..\build"
#endif
#define AppVer "1.0.0"

[Setup]
AppId={{57F3B752-1C27-4094-8BB1-A1F2669F30F0}
AppName=IsoDeck
AppVersion={#AppVer}
AppPublisher=FLOW POINT
AppPublisherURL=https://github.com/devhaider8354/IsoDeck
DefaultDirName={autopf}\IsoDeck
DefaultGroupName=IsoDeck
UninstallDisplayIcon={app}\IsoDeck.exe
OutputDir=..\dist
OutputBaseFilename=IsoDeck-Setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
LicenseFile=..\LICENSE

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
; Arabic UI file is optional: Name: "arabic"; MessagesFile: "compiler:Languages\Arabic.isl"

[Files]
Source: "{#BuildDir}\IsoDeck.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\IsoDeck"; Filename: "{app}\IsoDeck.exe"
Name: "{autodesktop}\IsoDeck"; Filename: "{app}\IsoDeck.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Run]
Filename: "{app}\IsoDeck.exe"; Description: "{cm:LaunchProgram,IsoDeck}"; Flags: nowait postinstall skipifsilent shellexec
