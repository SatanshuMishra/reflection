<#
.SYNOPSIS
    Bundle GStreamer runtime DLLs by recursively walking PE import tables.

.DESCRIPTION
    Discovers all DLL dependencies of an executable using dumpbin /dependents
    (MSVC) or objdump -p (MSYS2 fallback). Copies non-system DLLs from
    specified search directories to the output directory. Also copies
    GStreamer plugin DLLs (loaded dynamically by gst_init).

    This replaces brittle hardcoded DLL lists that break when GStreamer
    changes transitive dependency versions (e.g., ffi-7.dll vs ffi-8.dll).

.PARAMETER ExePath
    Path to the built executable (e.g., build\Release\Reflection.exe).

.PARAMETER SearchDirs
    Directories to search for non-system DLLs (e.g., GStreamer bin/).

.PARAMETER OutputDir
    Directory to copy discovered DLLs into (next to the executable).

.PARAMETER PluginSrcDir
    GStreamer plugin directory (e.g., lib/gstreamer-1.0/).

.PARAMETER PluginOutputDir
    Where to copy plugin DLLs (e.g., build/Release/plugins/).

.EXAMPLE
    .\bundle-gstreamer.ps1 `
        -ExePath "build\Release\Reflection.exe" `
        -SearchDirs "C:\gstreamer\1.0\msvc_x86_64\bin" `
        -OutputDir "build\Release" `
        -PluginSrcDir "C:\gstreamer\1.0\msvc_x86_64\lib\gstreamer-1.0" `
        -PluginOutputDir "build\Release\plugins"
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$ExePath,

    [Parameter(Mandatory)]
    [string[]]$SearchDirs,

    [Parameter(Mandatory)]
    [string]$OutputDir,

    [Parameter(Mandatory)]
    [string]$PluginSrcDir,

    [Parameter(Mandatory)]
    [string]$PluginOutputDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# ── Windows system DLL prefixes (never bundle these) ─────────────────────
$SystemDllPrefixes = @(
    'kernel32', 'kernelbase', 'ntdll', 'user32', 'gdi32', 'winspool',
    'advapi32', 'shell32', 'ole32', 'oleaut32', 'comctl32', 'comdlg32',
    'ws2_32', 'wsock32', 'iphlpapi', 'winhttp', 'wininet', 'wtsapi32',
    'secur32', 'bcrypt', 'crypt32', 'ncrypt', 'bcryptprimitives',
    'd3d11', 'd3d12', 'dxgi', 'd3dcompiler', 'd3d9',
    'mf', 'mfplat', 'mfreadwrite', 'mfuuid',
    'msvcrt', 'ucrtbase', 'vcruntime', 'msvcp',
    'setupapi', 'cfgmgr32', 'version', 'shlwapi', 'rpcrt4', 'imm32',
    'dwmapi', 'uxtheme', 'winmm', 'powrprof', 'userenv', 'netapi32',
    'dbghelp', 'psapi', 'normaliz', 'dnsapi', 'mswsock', 'propsys',
    'd2d1', 'd3d10', 'dwrite', 'dxcore', 'windowscodecs', 'shcore',
    'mmdevapi', 'avrt', 'audioses', 'resourcepolicyclient'
)

# Patterns that are always system DLLs
$SystemDllPatterns = @(
    '^api-ms-win-',
    '^ext-ms-win-',
    '^api-ms-onecoreuap-'
)

function Test-SystemDll {
    param([string]$DllName)
    $lower = $DllName.ToLower()

    foreach ($pattern in $SystemDllPatterns) {
        if ($lower -match $pattern) { return $true }
    }

    $nameWithoutExt = [System.IO.Path]::GetFileNameWithoutExtension($lower)
    foreach ($prefix in $SystemDllPrefixes) {
        # Exact match or versioned match (e.g., vcruntime140.dll)
        # Exact match, versioned suffix (vcruntime140.dll), or underscore-
        # versioned suffix (MSVCP140_ATOMIC_WAIT.dll, VCRUNTIME140_1.dll).
        if ($nameWithoutExt -eq $prefix -or
            $nameWithoutExt -match "^${prefix}\d{1,3}(_\w+)?$") {
            return $true
        }
    }

    return $false
}

# ── Locate dumpbin or objdump ────────────────────────────────────────────
$script:dumpbinPath = $null
$script:objdumpPath = $null

# Try vswhere to find dumpbin
$vswherePath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswherePath) {
    $vsInstallPath = & $vswherePath -latest -property installationPath 2>$null
    if ($vsInstallPath) {
        $candidates = Get-ChildItem "$vsInstallPath\VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe" -ErrorAction SilentlyContinue
        if ($candidates) {
            $script:dumpbinPath = ($candidates | Select-Object -Last 1).FullName
        }
    }
}

# Fallback: try dumpbin on PATH
if (-not $script:dumpbinPath) {
    $cmd = Get-Command dumpbin.exe -ErrorAction SilentlyContinue
    if ($cmd) { $script:dumpbinPath = $cmd.Source }
}

# Fallback: try objdump (MSYS2)
if (-not $script:dumpbinPath) {
    $cmd = Get-Command objdump.exe -ErrorAction SilentlyContinue
    if ($cmd) { $script:objdumpPath = $cmd.Source }
}

if (-not $script:dumpbinPath -and -not $script:objdumpPath) {
    Write-Error "Neither dumpbin.exe nor objdump.exe found. Install Visual Studio or MSYS2."
    exit 1
}

$toolName = if ($script:dumpbinPath) { "dumpbin ($script:dumpbinPath)" } else { "objdump ($script:objdumpPath)" }
Write-Host "Dependency walker: $toolName"

# ── Parse dependencies from a PE file ────────────────────────────────────
function Get-PeDependencies {
    param([string]$FilePath)

    $deps = @()
    if ($script:dumpbinPath) {
        $output = & $script:dumpbinPath /dependents $FilePath 2>$null
        foreach ($line in $output) {
            # dumpbin output: lines with just a DLL name (indented)
            if ($line -match '^\s+(\S+\.dll)\s*$') {
                $deps += $Matches[1]
            }
        }
    } elseif ($script:objdumpPath) {
        $output = & $script:objdumpPath -p $FilePath 2>$null
        foreach ($line in $output) {
            if ($line -match 'DLL Name:\s+(\S+\.dll)') {
                $deps += $Matches[1]
            }
        }
    }
    return $deps
}

# ── Resolve a DLL name to a file in search directories ───────────────────
function Resolve-DllPath {
    param(
        [string]$DllName,
        [string[]]$Directories
    )

    foreach ($dir in $Directories) {
        $candidate = Join-Path $dir $DllName
        if (Test-Path $candidate) {
            return $candidate
        }
    }
    return $null
}

# ── Recursive dependency walker ──────────────────────────────────────────
# Use $script: scope for state shared across recursive Walk-Dependencies calls.
# PowerShell functions can read parent-scope variables but hashtable/list
# mutations via method calls (.Add, .ContainsKey, [$key]=) in recursive
# functions require explicit $script: scope to avoid operating on local copies.
$script:visited = @{}
$script:copiedDlls = [System.Collections.Generic.List[string]]::new()
$script:unresolvedDlls = [System.Collections.Generic.List[string]]::new()

function Walk-Dependencies {
    param(
        [string]$FilePath,
        [string[]]$SearchDirectories
    )

    # Use the DLL filename (lowercased) as the visited key, not the full path.
    # This prevents walking the same logical DLL twice when it exists in both
    # the output directory and a search directory (different full paths, same DLL).
    $visitKey = [System.IO.Path]::GetFileName($FilePath).ToLower()
    if ($script:visited.ContainsKey($visitKey)) { return }
    $script:visited[$visitKey] = $true

    # @() ensures array context — PowerShell unrolls single-element returns
    $deps = @(Get-PeDependencies -FilePath $FilePath)
    foreach ($dep in $deps) {
        if (Test-SystemDll $dep) { continue }

        # Check if already in the output directory
        $inOutput = Join-Path $OutputDir $dep
        if (Test-Path $inOutput) {
            # Already there (vcpkg DLL or previously copied) -- still walk its deps
            Walk-Dependencies -FilePath $inOutput -SearchDirectories $SearchDirectories
            continue
        }

        $resolved = Resolve-DllPath -DllName $dep -Directories $SearchDirectories
        if ($resolved) {
            Copy-Item $resolved $inOutput -Force
            $script:copiedDlls.Add($dep)
            Write-Host "  Copied: $dep"
            Walk-Dependencies -FilePath $resolved -SearchDirectories $SearchDirectories
        } else {
            # Not in search dirs and not in output -- may be a system DLL we didn't filter
            $script:unresolvedDlls.Add($dep)
        }
    }
}

# ── Main ─────────────────────────────────────────────────────────────────

if (-not (Test-Path $ExePath)) {
    Write-Error "Executable not found: $ExePath"
    exit 1
}

# Validate search directories
foreach ($dir in $SearchDirs) {
    if (-not (Test-Path $dir)) {
        Write-Error "Search directory not found: $dir"
        exit 1
    }
}

Write-Host ""
Write-Host "=== GStreamer DLL Bundler ==="
Write-Host "Executable:  $ExePath"
Write-Host "Search dirs: $($SearchDirs -join '; ')"
Write-Host "Output dir:  $OutputDir"
Write-Host ""

# Include OutputDir itself in search paths so we walk deps of vcpkg DLLs too
$allSearchDirs = @($SearchDirs) + @($OutputDir)

Write-Host "--- Core DLL Discovery (import table walk) ---"
Walk-Dependencies -FilePath $ExePath -SearchDirectories $allSearchDirs

# ── Plugin DLLs (dynamically loaded by gst_init) ────────────────────────
Write-Host ""
Write-Host "--- GStreamer Plugin DLLs ---"

# Plugin names are feature-driven (stable across GStreamer versions).
# Check both MSVC naming (gst*.dll) and MSYS2 naming (libgst*.dll).
$pluginNames = @(
    'gstcoreelements'       # queue, tee, fakesink (GStreamer core)
    'gstapp'                # appsrc / appsink
    'gstvideoparsersbad'    # h264parse
    'gstd3d11'              # d3d11h264dec, d3d11videosink (GPU decode)
    'gstd3d'                # d3d memory allocators (GStreamer 1.24+)
    'gstlibav'              # avdec_h264, avdec_aac (FFmpeg software decode)
    'gstvideoconvertscale'  # videoconvert, videoscale
    'gstaudioconvert'       # audioconvert
    'gstaudioresample'      # audioresample
    'gstaudioparsers'       # aacparse
    'gstwasapi'             # wasapisink (Windows audio output, legacy)
    'gstwasapi2'            # wasapi2sink (Windows audio output, GStreamer 1.22+)
    'gstautodetect'         # autoaudiosink fallback
)

if (-not (Test-Path $PluginSrcDir)) {
    Write-Error "Plugin source directory not found: $PluginSrcDir"
    exit 1
}

New-Item -ItemType Directory -Path $PluginOutputDir -Force | Out-Null

$copiedPlugins = 0
$missingPlugins = [System.Collections.Generic.List[string]]::new()

foreach ($name in $pluginNames) {
    $found = $false
    # Try MSVC naming first, then MSYS2 naming
    foreach ($prefix in @('', 'lib')) {
        $dllName = "${prefix}${name}.dll"
        $srcPath = Join-Path $PluginSrcDir $dllName
        if (Test-Path $srcPath) {
            Copy-Item $srcPath (Join-Path $PluginOutputDir $dllName) -Force
            Write-Host "  Copied plugin: $dllName"
            $copiedPlugins++
            $found = $true

            # Walk plugin dependencies too (they may pull in additional DLLs)
            Walk-Dependencies -FilePath $srcPath -SearchDirectories $allSearchDirs
            break
        }
    }
    if (-not $found) {
        # Some plugins are optional depending on GStreamer version/variant:
        #   gstd3d      — only in GStreamer 1.24+ (d3d memory allocators)
        #   gstlibav    — FFmpeg decoders, only in MSYS2/MinGW builds (not MSVC)
        #                  d3d11h264dec is the primary decode path on MSVC
        #   gstwasapi   — legacy WASAPI plugin, may not ship in all MSVC builds
        #                  gstwasapi2 or gstautodetect provide fallback
        $optionalPlugins = @('gstd3d', 'gstlibav', 'gstwasapi')
        if ($name -in $optionalPlugins) {
            Write-Host "  Skipped optional plugin: $name (not found)"
        } else {
            $missingPlugins.Add($name)
        }
    }
}

# ── Summary ──────────────────────────────────────────────────────────────
Write-Host ""
Write-Host "=== Summary ==="
Write-Host "Core DLLs copied:   $($script:copiedDlls.Count)"
Write-Host "Plugins copied:     $copiedPlugins"

if ($script:unresolvedDlls.Count -gt 0) {
    Write-Host "Unresolved (likely system DLLs): $($script:unresolvedDlls -join ', ')"
}

if ($missingPlugins.Count -gt 0) {
    Write-Error "Missing required GStreamer plugins: $($missingPlugins -join ', ')"
    exit 1
}

Write-Host ""
Write-Host "DLL bundling complete."
