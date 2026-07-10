#define MyAppName "ED Screenshot"
#define MyAppVersion "0.2"
#define MyAppPublisher "Siegfried Origin"
#define MyAppURL "https://github.com/Siegfried-Origin/EDScreenshot"

[Setup]
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={code:GetGameDir}
DisableDirPage=yes 
DisableProgramGroupPage=yes
PrivilegesRequired=admin
OutputDir=Output
OutputBaseFilename=EDScreenshotInstaller
Compression=lzma2
SolidCompression=yes
Uninstallable=yes

[Files]
Source: "..\builds\x64\Release\d3d11.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "EDScreenshot.ini"; DestDir: "{app}\EDHM-ini\3rdPartyMods"; Flags: ignoreversion;

[UninstallDelete]
; We removed the backup DLL from here because we handle it manually in [Code] 
; to ensure it isn't deleted before we can restore it.
Type: files; Name: "{app}\EDHM-ini\3rdPartyMods\EDScreenshot.ini"

[Code]
var
  DetectionPage: TWizardPage;
  DetectionLabel: TNewStaticText;
  PathCombo: TComboBox; 

  DetectedPaths: TArrayOfString;
  PlatformNames: TArrayOfString;
  GamePath: String;
  FoundEDHM: Boolean;

// --- Detection Logic ---

function IsEliteFolder(Path: String): Boolean;
begin
  Result := FileExists(Path + '\EliteDangerous64.exe');
end;

procedure AddInstallation(Path, Platform: String);
begin
  if (Path <> '') and IsEliteFolder(Path) then begin
    SetLength(DetectedPaths, GetArrayLength(DetectedPaths) + 1);
    SetLength(PlatformNames, GetArrayLength(PlatformNames) + 1);
    DetectedPaths[GetArrayLength(DetectedPaths)-1] := Path;
    PlatformNames[GetArrayLength(PlatformNames)-1] := Platform;
  end;
end;

function GetSteamPath(): String;
begin
  Result := '';
  if not RegQueryStringValue(HKCU, 'Software\Valve\Steam', 'InstallPath', Result) then
    RegQueryStringValue(HKLM, 'SOFTWARE\WOW6432Node\Valve\Steam', 'InstallPath', Result);
end;

procedure SearchSteamLibrary(Path: String);
var
  Lines: TArrayOfString;
  I: Integer;
  Folder, Candidate: String;
begin
  if not LoadStringsFromFile(Path + '\steamapps\libraryfolders.vdf', Lines) then Exit;
  for I := 0 to GetArrayLength(Lines)-1 do begin
    if Pos('"path"', Lines[I]) > 0 then begin
      Folder := Lines[I];
      StringChangeEx(Folder, '"path"', '', True);
      StringChangeEx(Folder, '"', '', True);
      StringChangeEx(Folder, '\\', '\', True);
      Folder := Trim(Folder);
      Candidate := Folder + '\steamapps\common\Elite Dangerous\Products\elite-dangerous-odyssey-64';
      AddInstallation(Candidate, 'Steam');
    end;
  end;
end;

procedure FindEpicInstallations();
var 
  Lines: TArrayOfString;
  J: Integer;
  Path: String;
  FindRec: TFindRec;
  ManifestDir: String;
begin
  ManifestDir := ExpandConstant('{commonappdata}\Epic\EpicGamesLauncher\Data\Manifests');
  if not DirExists(ManifestDir) then Exit;
  if FindFirst(ManifestDir + '\*.item', FindRec) then begin
    try
      repeat 
        if LoadStringsFromFile(ManifestDir + '\' + FindRec.Name, Lines) then begin
          for J := 0 to GetArrayLength(Lines)-1 do begin
            if (Pos('Elite Dangerous', Lines[J]) > 0) and (Pos('InstallLocation', Lines[J]) > 0) then begin
              Path := Lines[J];
              StringChangeEx(Path, '"InstallLocation"', '', True);
              StringChangeEx(Path, '"', '', True);
              Path := Trim(Path);
              AddInstallation(Path, 'Epic Games');
            end;
          end;
        end;
      until not FindNext(FindRec);
    finally
      FindClose(FindRec);
    end;
  end;
end;

procedure FindFrontierInstallations();
begin
  AddInstallation(ExpandConstant('{pf}\Frontier Developments\Elite Dangerous'), 'Frontier');
  AddInstallation(ExpandConstant('{commonpf32}\Frontier Developments\Elite Dangerous'), 'Frontier');
end;

function GetGameDir(Param: String): String;
var SteamPath: String;
begin
  SetLength(DetectedPaths, 0);
  SetLength(PlatformNames, 0);

  SteamPath := GetSteamPath();
  if SteamPath <> '' then SearchSteamLibrary(SteamPath);
  FindEpicInstallations();
  FindFrontierInstallations();

  if GetArrayLength(DetectedPaths) > 0 then
    Result := DetectedPaths[0]
  else
    Result := '';
end;

// --- UI Logic ---

procedure UpdateDetectionPage();
var i: Integer;
begin
  PathCombo.Items.Clear();
  for i := 0 to GetArrayLength(DetectedPaths)-1 do begin
    PathCombo.Items.Add(PlatformNames[i] + ': ' + DetectedPaths[i]);
  end;

  if PathCombo.Items.Count > 0 then begin
    PathCombo.ItemIndex := 0;
    DetectionLabel.Caption := 'Elite Dangerous installations detected. Select one or paste your path below:';
  end else begin
    DetectionLabel.Caption := 'No installations found automatically. Please enter the game folder path manually:';
  end;
end;

procedure InitializeWizard();
begin
  DetectionPage := CreateCustomPage(wpSelectDir, 'Installation Path', 'Confirm game location');
  
  DetectionLabel := TNewStaticText.Create(WizardForm);
  DetectionLabel.Parent := DetectionPage.Surface;
  DetectionLabel.Left := 0;
  DetectionLabel.Top := 0;
  DetectionLabel.Width := DetectionPage.SurfaceWidth;
  DetectionLabel.Height := 60;
  DetectionLabel.WordWrap := True;

  PathCombo := TComboBox.Create(WizardForm);
  PathCombo.Parent := DetectionPage.Surface;
  PathCombo.Left := 0;
  PathCombo.Top := 70;
  PathCombo.Width := DetectionPage.SurfaceWidth;
  PathCombo.Style := csDropDown;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  SelectedText: String;
begin
  Result := True;
  if CurPageID = DetectionPage.ID then begin
    SelectedText := PathCombo.Text;
    
    if Pos(': ', SelectedText) > 0 then begin
       GamePath := Copy(SelectedText, Pos(': ', SelectedText) + 2, Length(SelectedText));
    end else begin
       GamePath := SelectedText;
    end;

    if (GamePath = '') or (not IsEliteFolder(GamePath)) then begin
      MsgBox('The folder "' + GamePath + '" is not a valid Elite Dangerous installation.'#13#10#13#10 + 
             'Please ensure it contains EliteDangerous64.exe', mbError, MB_OK);
      Result := False;
    end;
  end;
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = DetectionPage.ID then begin
    UpdateDetectionPage();
    if GetArrayLength(DetectedPaths) > 0 then
      FoundEDHM := DirExists(DetectedPaths[0] + '\EDHM-ini')
    else
      FoundEDHM := False;
  end;
end;

// --- Installation Steps ---

procedure BackupDLL();
var Src, Dst: String;
begin
  Src := ExpandConstant('{app}\d3d11.dll');
  Dst := ExpandConstant('{app}\d3d11.dll.edsbackup');
  // Backup only if the original exists and we haven't already backed it up
  if FileExists(Src) and not FileExists(Dst) then 
    FileCopy(Src, Dst, False);
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssInstall then begin
    // Run backup BEFORE files are copied/overwritten
    BackupDLL();
    
    if not FoundEDHM then begin
      if MsgBox('EDHM was not detected. ED Screenshot is designed to work with EDHM. Continue anyway?', mbConfirmation, MB_YESNO) = IDNO then Abort();
    end;
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var DLL, Backup: String;
begin
  // We use usPostUninstall so that Inno Setup has already deleted the modded d3d11.dll.
  // This allows us to put the original one back without it being immediately deleted.
  if CurUninstallStep = usPostUninstall then begin
    Backup := ExpandConstant('{app}\d3d11.dll.edsbackup');
    DLL := ExpandConstant('{app}\d3d11.dll');
    
    if FileExists(Backup) then begin
      // 1. Restore original DLL
      FileCopy(Backup, DLL, False);
      // 2. Now delete the backup file manually since it's no longer needed
      DeleteFile(Backup);
    end;
  end;
end;
