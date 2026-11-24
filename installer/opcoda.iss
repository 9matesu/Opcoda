; Opcoda - professional installer (Inno Setup 6).
;
;   powershell -File tools\build-installer.ps1
;
; Version, sizes and payload location arrive as /D defines from the build
; script; the defaults below allow manual compilation.
;
; Supported content: Windows x64, VST3 + Standalone. VST2 / AU / AAX builds
; do not exist in this project and are intentionally NOT offered.

#define MyAppName "Opcoda"
#ifndef MyAppVersion
  #define MyAppVersion "0.1.1"
#endif
#ifndef TotalSize
  #define TotalSize "0 MB"
#endif
#ifndef Vst3Size
  #define Vst3Size "0 MB"
#endif
#ifndef StandaloneSize
  #define StandaloneSize "0 MB"
#endif
#ifndef DocsSize
  #define DocsSize "0 MB"
#endif
#ifndef PayloadDir
  #define PayloadDir "stage"
#endif

[Setup]
AppId={{B4E2C1A0-7D3F-4C8A-9E51-2F6A0D9C4B77}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher=bloodwormtrax
AppPublisherURL=https://bloodwormtrax.neocities.org
AppSupportURL=https://bloodwormtrax.neocities.org
AppUpdatesURL=https://bloodwormtrax.neocities.org
AppCopyright=Copyright (C) 2026 bloodwormtrax
VersionInfoCompany=bloodworm trax
VersionInfoDescription={#MyAppName} Setup
VersionInfoProductName={#MyAppName}
VersionInfoProductVersion={#MyAppVersion}
VersionInfoVersion={#MyAppVersion}
DefaultDirName={commonpf}\Opcoda
DefaultGroupName=Opcoda
DisableProgramGroupPage=yes
UninstallDisplayName={#MyAppName}
UninstallDisplayIcon={app}\Opcoda.exe
OutputDir=Output
OutputBaseFilename=Opcoda-Setup-{#MyAppVersion}
SetupIconFile=OpcodaSetup.ico
WizardImageFile=WizModernImage.bmp
WizardSmallImageFile=WizModernSmallImage.bmp
LicenseFile=LICENSE.txt
Compression=zip
SolidCompression=yes
WizardStyle=modern
ShowLanguageDialog=no
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=commandline
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
SetupLogging=yes
Uninstallable=yes
CreateUninstallRegKey=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Components]
Name: "vst3"; Description: "VST3 Plugin (64-bit) - Opcoda.vst3 for your DAW"; Flags: checkablealone
Name: "standalone"; Description: "Standalone Application - play and design without a DAW"; Flags: checkablealone
Name: "docs"; Description: "User Manual (PDF) - every parameter explained"; Flags: checkablealone

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"; Components: standalone
Name: "vcredist"; Description: "Install the Microsoft Visual C++ 2015-2022 runtime (recommended for clean systems)"; GroupDescription: "Dependencies:"; Check: not VCRedistInstalled()

[Files]
Source: "{#PayloadDir}\Opcoda.vst3\*"; DestDir: "{code:Vst3Dir}"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#PayloadDir}\Opcoda.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "{#PayloadDir}\Docs\Opcoda-Manual.pdf"; DestDir: "{app}\Docs"; Components: docs; Flags: ignoreversion
Source: "redist\vc_redist.x64.exe"; DestDir: "{tmp}"; Tasks: vcredist

[Icons]
Name: "{group}\Opcoda"; Filename: "{app}\Opcoda.exe"; Components: standalone
Name: "{group}\Documentation"; Filename: "{app}\Docs\Opcoda-Manual.pdf"; Components: docs
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Opcoda"; Filename: "{app}\Opcoda.exe"; Tasks: desktopicon; Components: standalone

[Run]
Filename: "{app}\Opcoda.exe"; Description: "Launch {#MyAppName}"; Flags: nowait postinstall skipifsilent; Components: standalone
Filename: "{app}\Docs\Opcoda-Manual.pdf"; Description: "Open the documentation"; Flags: shellexec postinstall skipifsilent; Components: docs
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "Installing the Microsoft Visual C++ runtime..."; Tasks: vcredist; Flags: waituntilterminated runhidden

[Registry]
Root: HKA; Subkey: "Software\bloodworm trax\Opcoda"; Flags: dontcreatekey uninsdeletekey

[Code]
var
  Vst3DirPage: TInputDirWizardPage;
  RestoreDefaultsButton: TNewButton;
  SizeLabel: TNewStaticText;

function VCRedistInstalled(): Boolean;
begin
  Result := RegKeyExists(HKLM, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64') or
            RegKeyExists(HKLM, 'SOFTWARE\WOW6432Node\Microsoft\VisualStudio\14.0\VC\Runtimes\x64') or
            FileExists(ExpandConstant('{sys}\vcruntime140.dll'));
end;

procedure RestoreDefaultsClick(Sender: TObject);
begin
  Vst3DirPage.Values[0] := ExpandConstant('{autocf}\VST3');
end;

procedure InitializeWizard();
begin
  Vst3DirPage := CreateInputDirPage(wpSelectComponents,
    'Plugin Paths',
    'Where should the plugins and the application be installed? DAWs scan the Common Files\VST3 folder. Choose the destination for Opcoda.vst3.',
    'Plugin folder', True, '');
  Vst3DirPage.Add('');
  Vst3DirPage.Values[0] := ExpandConstant('{autocf}\VST3');

  RestoreDefaultsButton := TNewButton.Create(WizardForm);
  RestoreDefaultsButton.Parent := Vst3DirPage.Surface;
  RestoreDefaultsButton.Left := ScaleX(12);
  RestoreDefaultsButton.Top := Vst3DirPage.Surface.Height - ScaleY(34);
  RestoreDefaultsButton.Width := ScaleX(140);
  RestoreDefaultsButton.Height := ScaleY(23);
  RestoreDefaultsButton.Caption := 'Restore defaults';
  RestoreDefaultsButton.OnClick := @RestoreDefaultsClick;

  SizeLabel := TNewStaticText.Create(WizardForm);
  SizeLabel.Parent := WizardForm.MainPanel;
  SizeLabel.Left := ScaleX(14);
  SizeLabel.Width := WizardForm.MainPanel.Width - ScaleX(28);
  SizeLabel.Height := ScaleY(16);
  SizeLabel.Top := WizardForm.MainPanel.Height - ScaleY(26);
  SizeLabel.Caption := 'Total package size: {#TotalSize}  (VST3 {#Vst3Size} / Standalone {#StandaloneSize} / Docs {#DocsSize})';
  SizeLabel.Visible := False;
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  SizeLabel.Visible := CurPageID = wpSelectComponents;
end;

function ShouldSkipPage(CurPageID: Integer): Boolean;
begin
  Result := (CurPageID = Vst3DirPage.ID) and (not WizardIsComponentSelected('vst3'));
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  dir: String;
begin
  Result := True;
  if CurPageID <> Vst3DirPage.ID then
    Exit;
  dir := Vst3DirPage.Values[0];
  if Trim(dir) = '' then
  begin
    MsgBox('Please choose a folder for the VST3 plugin.', mbError, MB_OK);
    Result := False;
    Exit;
  end;
  if not DirExists(dir) then
    if not ForceDirectories(dir) then
    begin
      MsgBox('Cannot create or access the folder:' + #13#10 + dir + #13#10#13#10 +
             'Check the path and your permissions.', mbError, MB_OK);
      Result := False;
      Exit;
    end;
  if CompareText(ExtractFileName(Trim(dir)), 'VST3') <> 0 then
    if MsgBox('DAWs usually scan folders named "VST3". Your choice does not end in "VST3". Continue anyway?',
              mbConfirmation, MB_YESNO) = IDNO then
      Result := False;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    RegWriteStringValue(HKA, 'Software\bloodworm trax\Opcoda', 'LicenseAcceptedVersion', '{#MyAppVersion}');
    RegWriteStringValue(HKA, 'Software\bloodworm trax\Opcoda', 'LicenseAcceptedAt', GetDateTimeString('yyyy-mm-dd hh:nn:ss', '-', ':'));
  end;
end;

function Vst3Dir(Param: String): String;
var
  i: Integer;
  prefix: String;
begin
  prefix := '/VST3DIR=';
  for i := 1 to ParamCount do
    if Pos(prefix, ParamStr(i)) = 1 then
      Result := Copy(ParamStr(i), Length(prefix) + 1, MaxInt);
  if Result = '' then
    Result := Vst3DirPage.Values[0];
end;