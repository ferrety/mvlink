[CmdletBinding()]
param([Parameter(Mandatory)][string]$Package)

$ErrorActionPreference = 'Stop'
$Package = (Resolve-Path $Package).Path
$exe = Join-Path $Package 'mvlink.exe'
$version = (Get-Content (Join-Path $Package 'version.txt') -Raw).Trim()
$metadata = (Get-Item $exe).VersionInfo
if ($metadata.ProductVersion -ne "$version.0") {
    throw "Product version '$($metadata.ProductVersion)' does not match $version"
}
if ($metadata.FileVersion -ne "$version.0") {
    throw "File version '$($metadata.FileVersion)' does not match $version.0"
}
foreach ($file in @('Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll', 'platforms/qwindows.dll', 'qt.conf')) {
    if (-not (Test-Path (Join-Path $Package $file))) { throw "Package missing $file" }
}

$oldPath = $env:PATH
$oldPluginPath = $env:QT_PLUGIN_PATH
$oldPlatformPath = $env:QT_QPA_PLATFORM_PLUGIN_PATH
$oldPlatform = $env:QT_QPA_PLATFORM
$temp = Join-Path ([IO.Path]::GetTempPath()) ([guid]::NewGuid().ToString())
New-Item -ItemType Directory $temp | Out-Null
try {
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
    $env:QT_PLUGIN_PATH = $null
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = $null
    $env:QT_QPA_PLATFORM = $null
    $stdout = Join-Path $temp 'version.txt'
    $process = Start-Process $exe -ArgumentList '--version' -PassThru -WorkingDirectory $temp `
        -RedirectStandardOutput $stdout -RedirectStandardError (Join-Path $temp 'version-error.txt')
    # Retain the process handle so ExitCode is available even after a fast exit.
    $null = $process.Handle
    if (-not $process.WaitForExit(15000)) {
        $process.Kill()
        throw 'Version query timed out'
    }
    if ($process.ExitCode -ne 0 -or (Get-Content $stdout -Raw).Trim() -ne "mvlink $version") {
        throw 'Packaged version query failed'
    }
    $process = Start-Process $exe -PassThru -WorkingDirectory $temp `
        -RedirectStandardError (Join-Path $temp 'startup-error.txt')
    try {
        if ($process.WaitForExit(5000)) {
            throw "Packaged GUI exited early: $(Get-Content (Join-Path $temp 'startup-error.txt') -Raw)"
        }
        $process.Refresh()
        if ($process.MainWindowHandle -eq 0) { throw 'Packaged GUI did not create a window' }
    } finally {
        if (-not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
    }
} finally {
    $env:PATH = $oldPath
    $env:QT_PLUGIN_PATH = $oldPluginPath
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = $oldPlatformPath
    $env:QT_QPA_PLATFORM = $oldPlatform
    Remove-Item -Recurse -Force $temp
}
