<#
.SYNOPSIS
    Remove all Reflection artifacts from the system.

.DESCRIPTION
    Standalone cleanup script that removes all traces of Reflection without
    requiring the application executable or installer. Use this when:
      - The Inno Setup uninstaller fails or is unavailable
      - Dev builds left behind artifacts (registry, firewall, WebView2)
      - Testing installer changes requires a pristine system state
      - Portable installations need cleanup after folder deletion

    This script mirrors the cleanup performed by:
      - Reflection.exe --uninstall-cleanup (C++ reset_all_data + reset_firewall)
      - Inno Setup [UninstallRun], [UninstallDelete], [Registry] sections

    Requires elevation (Run as Administrator) for firewall rule removal.

.PARAMETER Force
    Skip the confirmation prompt.

.PARAMETER SkipFirewall
    Skip firewall rule removal (avoids UAC prompt when not elevated).

.EXAMPLE
    .\clean-reflection.ps1
    .\clean-reflection.ps1 -Force
    .\clean-reflection.ps1 -SkipFirewall
#>

[CmdletBinding()]
param(
    [switch]$Force,
    [switch]$SkipFirewall
)

$ErrorActionPreference = 'Continue'

Write-Host ""
Write-Host "=============================================="
Write-Host "  Reflection -- System Cleanup"
Write-Host "=============================================="
Write-Host ""

if (-not $Force) {
    Write-Host "  This will remove ALL Reflection data from this system:"
    Write-Host "    - Registry settings (HKCU\Software\Reflection)"
    Write-Host "    - Registry settings (HKCU\Software\Reflection-Dev)"
    Write-Host "    - Auto-start entries"
    Write-Host "    - WebView2 browser cache"
    Write-Host "    - Windows Firewall rules"
    Write-Host "    - Start Menu shortcuts"
    Write-Host "    - Desktop shortcut"
    Write-Host "    - Installation directory (if present)"
    Write-Host ""
    $confirm = Read-Host "  Continue? [y/N]"
    if ($confirm -ne 'y' -and $confirm -ne 'Y') {
        Write-Host "  Cancelled."
        exit 0
    }
}

$cleaned = @()
$failed = @()

# ── 1. Registry: Application settings ────────────────────────────────────
Write-Host ""
Write-Host "--- Registry ---"

foreach ($instance in @('Reflection', 'Reflection-Dev')) {
    $regPath = "HKCU:\Software\$instance"
    if (Test-Path $regPath) {
        try {
            Remove-Item $regPath -Recurse -Force
            Write-Host "  Removed: $regPath"
            $cleaned += "Registry: $instance"
        } catch {
            Write-Host "  FAILED: $regPath -- $_"
            $failed += "Registry: $instance"
        }
    } else {
        Write-Host "  Clean: $regPath (not found)"
    }
}

# ── 2. Registry: Auto-start entries ─────────────────────────────────────
Write-Host ""
Write-Host "--- Auto-Start ---"

$runKeyPath = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run"
foreach ($valueName in @('Reflection', 'Reflection-Dev')) {
    $existing = Get-ItemProperty $runKeyPath -Name $valueName -ErrorAction SilentlyContinue
    if ($existing) {
        try {
            Remove-ItemProperty $runKeyPath -Name $valueName -Force
            Write-Host "  Removed: Run\$valueName"
            $cleaned += "Auto-start: $valueName"
        } catch {
            Write-Host "  FAILED: Run\$valueName -- $_"
            $failed += "Auto-start: $valueName"
        }
    } else {
        Write-Host "  Clean: Run\$valueName (not found)"
    }
}

# ── 3. Firewall rules ───────────────────────────────────────────────────
Write-Host ""
Write-Host "--- Firewall Rules ---"

if ($SkipFirewall) {
    Write-Host "  Skipped (--SkipFirewall)"
} else {
    foreach ($ruleName in @('Reflection', 'Reflection-Dev')) {
        $fwCheck = netsh advfirewall firewall show rule name="$ruleName" 2>&1
        $ruleCount = @($fwCheck | Select-String "Rule Name:").Count
        if ($ruleCount -gt 0) {
            $result = netsh advfirewall firewall delete rule name="$ruleName" 2>&1
            if ($LASTEXITCODE -eq 0) {
                Write-Host "  Removed: $ruleCount rule(s) named '$ruleName'"
                $cleaned += "Firewall: $ruleName ($ruleCount rules)"
            } else {
                Write-Host "  FAILED: '$ruleName' -- may need elevation (Run as Administrator)"
                $failed += "Firewall: $ruleName (needs admin)"
            }
        } else {
            Write-Host "  Clean: '$ruleName' (no rules found)"
        }
    }
}

# ── 4. WebView2 / App data ──────────────────────────────────────────────
Write-Host ""
Write-Host "--- WebView2 / App Data ---"

foreach ($subdir in @('Reflection', 'Reflection-Dev')) {
    $appDataPath = Join-Path $env:LOCALAPPDATA $subdir
    if (Test-Path $appDataPath) {
        $fileCount = @(Get-ChildItem $appDataPath -Recurse -File -ErrorAction SilentlyContinue).Count
        try {
            Remove-Item $appDataPath -Recurse -Force
            Write-Host "  Removed: $appDataPath ($fileCount files)"
            $cleaned += "AppData: $subdir"
        } catch {
            Write-Host "  FAILED: $appDataPath -- $_"
            $failed += "AppData: $subdir"
        }
    } else {
        Write-Host "  Clean: $appDataPath (not found)"
    }
}

# ── 5. Installation directory ────────────────────────────────────────────
Write-Host ""
Write-Host "--- Installation Directory ---"

foreach ($installPath in @("$env:ProgramFiles\Reflection", "${env:ProgramFiles(x86)}\Reflection")) {
    if (Test-Path $installPath) {
        try {
            Remove-Item $installPath -Recurse -Force
            Write-Host "  Removed: $installPath"
            $cleaned += "InstallDir: $installPath"
        } catch {
            Write-Host "  FAILED: $installPath -- $_ (may need elevation)"
            $failed += "InstallDir: $installPath"
        }
    } else {
        Write-Host "  Clean: $installPath (not found)"
    }
}

# ── 6. Start Menu shortcuts ─────────────────────────────────────────────
Write-Host ""
Write-Host "--- Start Menu ---"

foreach ($smPath in @(
    "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\Reflection",
    "$env:ProgramData\Microsoft\Windows\Start Menu\Programs\Reflection"
)) {
    if (Test-Path $smPath) {
        try {
            Remove-Item $smPath -Recurse -Force
            Write-Host "  Removed: $smPath"
            $cleaned += "StartMenu"
        } catch {
            Write-Host "  FAILED: $smPath -- $_"
            $failed += "StartMenu"
        }
    }
}

# ── 7. Desktop shortcut ─────────────────────────────────────────────────
$desktopLnk = "$env:USERPROFILE\Desktop\Reflection.lnk"
if (Test-Path $desktopLnk) {
    Remove-Item $desktopLnk -Force
    Write-Host "  Removed: Desktop shortcut"
    $cleaned += "Desktop shortcut"
}

# ── 8. Inno Setup uninstaller registry ──────────────────────────────────
Write-Host ""
Write-Host "--- Inno Setup Uninstaller ---"

$uninstId = "{BCF3A179-BA9E-4D4E-A3BB-129A81A5E57A}_is1"
foreach ($root in @("HKCU:", "HKLM:")) {
    $uninstPath = "$root\Software\Microsoft\Windows\CurrentVersion\Uninstall\$uninstId"
    if (Test-Path $uninstPath) {
        try {
            Remove-Item $uninstPath -Recurse -Force
            Write-Host "  Removed: $uninstPath"
            $cleaned += "Uninstaller registry"
        } catch {
            Write-Host "  FAILED: $uninstPath -- $_"
            $failed += "Uninstaller registry"
        }
    }
}

# ── Summary ──────────────────────────────────────────────────────────────
Write-Host ""
Write-Host "=============================================="
Write-Host "  SUMMARY"
Write-Host "=============================================="
Write-Host ""

if ($cleaned.Count -gt 0) {
    Write-Host "Cleaned: $($cleaned.Count) items"
    foreach ($c in $cleaned) { Write-Host "  $c" }
}

if ($failed.Count -gt 0) {
    Write-Host ""
    Write-Host "FAILED: $($failed.Count) items (may need 'Run as Administrator')"
    foreach ($f in $failed) { Write-Host "  $f" }
    exit 1
}

if ($cleaned.Count -eq 0) {
    Write-Host "System was already clean -- nothing to remove."
}

Write-Host ""
Write-Host "Done."
