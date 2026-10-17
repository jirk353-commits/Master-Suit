; Setup wizard for ROHIT JI plugins (Inno Setup 6). One installer per architecture.
; GitHub Actions calls it with:
;   /DAppName="Opto Glow" /DArch=64 (or 32) /DAppIdGuid=... /DArtDir=...\Installer /DPluginDir=... /DOutName=...

#define AppVersion   "1.0.0"
#define AppPublisher "ROHIT JI"
#define AppIdFull    "{{" + AppIdGuid + "}"
#define ArchLabel    Arch + "-bit"

[Setup]
AppId={#AppIdFull}
AppName={#AppName} ({#ArchLabel})
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion} ({#ArchLabel})
AppPublisher={#AppPublisher}
VersionInfoVersion={#AppVersion}
VersionInfoCompany={#AppPublisher}
VersionInfoDescription={#AppName} {#ArchLabel} Setup
VersionInfoProductName={#AppName} ({#ArchLabel})
#if Arch == "64"
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
DefaultDirName={commoncf64}\VST3
#else
ArchitecturesAllowed=x86compatible x64compatible
DefaultDirName={commoncf32}\VST3
#endif
DisableDirPage=yes
DisableProgramGroupPage=yes
Uninstallable=yes
UninstallDisplayName={#AppName} {#ArchLabel} (VST3) by {#AppPublisher}
UninstallDisplayIcon={uninstallexe}
UninstallFilesDir={app}\..\{#AppPublisher}\{#AppName} {#ArchLabel}
PrivilegesRequired=admin
OutputBaseFilename={#OutName}
SetupIconFile={#ArtDir}\icon.ico
WizardStyle=modern
WizardImageFile={#ArtDir}\wizard-large-100.bmp,{#ArtDir}\wizard-large.bmp
WizardSmallImageFile={#ArtDir}\wizard-small-100.bmp,{#ArtDir}\wizard-small.bmp
WizardImageStretch=yes
Compression=lzma2/max
SolidCompression=yes
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Messages]
SetupWindowTitle=Setup - {#AppName} ({#ArchLabel})
WelcomeLabel1=Welcome to the {#AppName} Setup%n({#ArchLabel} VST3)
#if Arch == "64"
WelcomeLabel2=This will install {#AppName} {#AppVersion} by ROHIT JI - 64-bit VST3 version.%n%nUse this for 64-bit FL Studio, Cubase and other modern DAWs.%n%nPlease close your music software before continuing.
#else
WelcomeLabel2=This will install {#AppName} {#AppVersion} by ROHIT JI - 32-bit VST3 version.%n%nUse this only for older 32-bit DAWs. For modern 64-bit FL Studio / Cubase use the 64-bit setup instead.%n%nPlease close your music software before continuing.
#endif
FinishedHeadingLabel={#AppName} ({#ArchLabel}) is installed!
FinishedLabelNoIcons=The plugin is ready to use.%n%nFL STUDIO: Options > Manage plugins > Find installed plugins.%n%nCUBASE: restart Cubase (or Studio > VST Plug-in Manager > Rescan).

[InstallDelete]
Type: filesandordirs; Name: "{app}\{#AppName}.vst3"
#ifdef OldName
Type: filesandordirs; Name: "{app}\{#OldName}.vst3"
#endif

[Files]
Source: "{#PluginDir}\*"; DestDir: "{app}\{#AppName}.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: filesandordirs; Name: "{app}\{#AppName}.vst3"
