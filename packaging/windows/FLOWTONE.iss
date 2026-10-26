; Inno Setup 6.3+, production x64 VST3 installer.
[Setup]
AppId={{CE3AE3DB-A410-4AF5-BCEA-EFBF28B1C8FE}
AppName=FLOWTONE
AppVersion={#PluginVersion}
AppPublisher=FLOWMINNERS
DefaultDirName={autopf}\FLOWMINNERS\FLOWTONE
DisableDirPage=yes
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64os
ArchitecturesInstallIn64BitMode=x64os
MinVersion=10.0
PrivilegesRequired=admin
OutputDir={#DistDir}
OutputBaseFilename=FLOWTONE-{#PluginVersion}-Windows-x64-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=FLOWTONE VST3
CloseApplications=no
RestartApplications=no
SetupLogging=yes

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Messages]
russian.WelcomeLabel2=Установщик добавит FLOWTONE VST3 для 64-разрядных секвенсоров Windows 10 и 11.%n%nСохраните проекты и закройте FL Studio и другие аудиопрограммы перед продолжением.%n%nПредыдущая версия плагина будет скопирована в резервную папку. Аудиофайлы и проекты не изменяются.

[Files]
Source: "{#PluginDir}\*"; DestDir: "{commoncf64}\VST3\FLOWTONE.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#DocPath}"; DestDir: "{app}"; DestName: "Установка.txt"; Flags: ignoreversion

[Code]
var
  Prepared: Boolean;

function CopyBackup(const Source, Dest: String): Boolean;
var
  Entry: TFindRec;
  FromName, ToName: String;
begin
  Result := ForceDirectories(Dest);
  if not Result then Exit;
  if FindFirst(Source + '\*', Entry) then
  begin
    try
      repeat
        if (Entry.Name <> '.') and (Entry.Name <> '..') then
        begin
          FromName := Source + '\' + Entry.Name;
          ToName := Dest + '\' + Entry.Name;
          if (Entry.Attributes and FILE_ATTRIBUTE_DIRECTORY) <> 0 then
            Result := CopyBackup(FromName, ToName)
          else
          begin
            Result := FileCopy(FromName, ToName, True);
            if Result then Result := CompareText(GetSHA256OfFile(FromName), GetSHA256OfFile(ToName)) = 0;
          end;
          if not Result then Exit;
        end;
      until not FindNext(Entry);
    finally
      FindClose(Entry);
    end;
  end;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  Existing, Backup, BaseBackup: String;
  Suffix: Integer;
begin
  Result := '';
  if Prepared then Exit;
  Existing := ExpandConstant('{commoncf64}\VST3\FLOWTONE.vst3');
  if DirExists(Existing) then
  begin
    BaseBackup := ExpandConstant('{commonappdata}\FLOWMINNERS\FLOWTONE\Backups\') +
      GetDateTimeString('yyyymmdd-hhnnss', '-', ':');
    Backup := BaseBackup;
    Suffix := 0;
    while DirExists(Backup) do
    begin
      Suffix := Suffix + 1;
      Backup := BaseBackup + '-' + IntToStr(Suffix);
    end;
    Backup := Backup + '\FLOWTONE.vst3';
    if not CopyBackup(Existing, Backup) then
    begin
      Result := 'Не удалось проверить резервную копию FLOWTONE. Закройте аудиопрограммы и проверьте доступ к папке ProgramData.';
      Exit;
    end;
    Log('Verified backup: ' + Backup);
  end;
  Prepared := True;
end;

#include ManifestInclude

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then VerifyPayload;
end;
