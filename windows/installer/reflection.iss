; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Satanshu Mishra
;
; Inno Setup script for Reflection -- AirPlay screen mirroring for Windows.
; Compiles with: iscc /DAppVersion=1.6.0 reflection.iss
;
; Build artifacts are expected in ..\build\Release\ relative to this script.
; The release workflow (release.yml) compiles this after CMake build.

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif

#define AppName      "Reflection"
#define AppPublisher "Satanshu Mishra"
#define AppURL       "https://github.com/SatanshuMishra/reflection"
#define AppExeName   "Reflection.exe"

; Build output directory (relative to this .iss file)
#define BuildDir     "..\build\Release"

[Setup]
AppId={{BCF3A179-BA9E-4D4E-A3BB-129A81A5E57A}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}/issues
AppUpdatesURL={#AppURL}/releases
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
OutputDir=Output
OutputBaseFilename={#AppName}-{#AppVersion}-Setup
SetupIconFile=..\src\resources\app.ico
LicenseFile=..\..\LICENSE
; Admin required for Program Files and firewall rule
PrivilegesRequired=admin
Compression=lzma2/ultra64
SolidCompression=yes
; 64-bit only
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Uninstall icon from the installed exe
UninstallDisplayIcon={app}\{#AppExeName}
UninstallDisplayName={#AppName}
; Visual polish
WizardStyle=modern
DisableProgramGroupPage=yes
; Close running instances before install/uninstall via Restart Manager.
; Defense-in-depth: [UninstallRun] also force-kills the process.
CloseApplications=force
CloseApplicationsFilter=*.exe,*.dll
; Allow user to choose custom install dir
AllowNoIcons=yes
; Desktop shortcut and HKCU registry are intentionally per-user
UsedUserAreasWarning=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Main executable
Source: "{#BuildDir}\{#AppExeName}"; DestDir: "{app}"; Flags: ignoreversion

; GStreamer runtime DLLs (co-located with exe)
Source: "{#BuildDir}\*.dll"; DestDir: "{app}"; Flags: ignoreversion

; GStreamer plugin DLLs (only present in MSYS2 local builds, not CI MSVC builds)
Source: "{#BuildDir}\plugins\*"; DestDir: "{app}\plugins"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

; UI assets (WebView2 HTML/CSS/JS)
Source: "{#BuildDir}\assets\*"; DestDir: "{app}\assets"; Flags: ignoreversion recursesubdirs createallsubdirs

; License file for reference
Source: "..\..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion

[Icons]
; Start Menu shortcut
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Comment: "Mirror your iPad screen wirelessly"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"
; Desktop shortcut (optional)
Name: "{userdesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon; Comment: "Mirror your iPad screen wirelessly"

[Run]
; Configure Windows Firewall inbound rule for AirPlay connections.
; Delete-then-add ensures no duplicate rules accumulate across
; reinstalls or when the app's UI also creates a rule at runtime.
Filename: "netsh.exe"; \
  Parameters: "advfirewall firewall delete rule name=""{#AppName}"""; \
  Flags: runhidden; \
  StatusMsg: "Configuring Windows Firewall..."

Filename: "netsh.exe"; \
  Parameters: "advfirewall firewall add rule name=""{#AppName}"" dir=in action=allow program=""{app}\{#AppExeName}"" enable=yes profile=private,public"; \
  Flags: runhidden; \
  StatusMsg: "Configuring Windows Firewall..."

; Optionally launch the app after install
Filename: "{app}\{#AppExeName}"; \
  Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; \
  Flags: nowait postinstall skipifsilent

[UninstallRun]
; Step 0: Force-terminate any running instances.
; The app may be running from postinstall launch or system tray.
; CloseApplications=force handles most cases via Restart Manager, but
; taskkill is defense-in-depth for edge cases (tray icon, hung process).
; /F = force, /IM = image name. Exit code 128 = "not found" (benign).
Filename: "taskkill.exe"; \
  Parameters: "/F /IM {#AppExeName}"; \
  Flags: runhidden; \
  RunOnceId: "KillRunningApp"

; Step 1: Run the app's own cleanup (registry, WebView2 cache, log files).
; NOTE: --uninstall-cleanup does NOT reset the firewall rule (Step 2
; handles it directly with admin privileges, avoiding a redundant UAC prompt).
Filename: "{app}\{#AppExeName}"; \
  Parameters: "--uninstall-cleanup"; \
  Flags: runhidden; \
  RunOnceId: "AppCleanup"

; Step 2: Remove firewall rule. Uses name-only match (no program filter)
; so it catches rules created by the installer AND by the app's UI,
; regardless of which exe path was used when the rule was created.
Filename: "netsh.exe"; \
  Parameters: "advfirewall firewall delete rule name=""{#AppName}"""; \
  Flags: runhidden; \
  RunOnceId: "RemoveFirewallRule"

[UninstallDelete]
; Clean up log files and any runtime-generated data
Type: files; Name: "{app}\reflection.log"
Type: files; Name: "{app}\reflection.log.*"
Type: files; Name: "{app}\Reflection.log"
Type: files; Name: "{app}\Reflection.log.*"
; Remove plugins and assets dirs (installer created them)
Type: filesandordirs; Name: "{app}\plugins"
Type: filesandordirs; Name: "{app}\assets"
; Remove the install directory itself if anything remains (defense-in-depth).
; Inno Setup normally removes {app} only if empty. This catches runtime-
; generated files that the installer didn't track.
Type: filesandordirs; Name: "{app}"
; Remove WebView2 user data (browser cache, cookies, localStorage)
Type: filesandordirs; Name: "{localappdata}\{#AppName}"

[Registry]
; Application settings — delete entire key tree on uninstall.
; This removes OnboardingCompleted, ServerName, Theme, FirewallConfigured,
; window position, and all other app settings. Ensures reinstall starts fresh.
Root: HKCU; Subkey: "Software\{#AppName}"; Flags: uninsdeletekey
; Store install path (auto-deleted by uninsdeletekey above)
Root: HKCU; Subkey: "Software\{#AppName}"; ValueType: string; ValueName: "InstallPath"; ValueData: "{app}"
; Auto-start entry — dontcreatekey means installer won't create it (the app
; creates it via Settings UI), but uninsdeletevalue ensures it's removed on uninstall.
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "{#AppName}"; Flags: uninsdeletevalue dontcreatekey
