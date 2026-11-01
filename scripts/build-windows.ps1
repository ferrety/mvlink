[CmdletBinding()]
param(
    [string]$QtRoot = $env:QT_ROOT_DIR,
    [string]$MingwRoot,
    [switch]$Test
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
if (-not $QtRoot) { throw 'Pass -QtRoot pointing to the Qt 6.8.3 mingw_64 directory.' }
$QtRoot = (Resolve-Path $QtRoot).Path
if (-not $MingwRoot) {
    $MingwRoot = Split-Path (Split-Path (Get-Command g++.exe -ErrorAction Stop).Source)
}
$MingwRoot = (Resolve-Path $MingwRoot).Path
$env:PATH = "$QtRoot\bin;$MingwRoot\bin;$env:PATH"

function Invoke-Native([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}

# Reuse the same script locally and on CI, without mixing with Docker output.
$build = Join-Path $repo 'build/windows-native'
$package = Join-Path $repo 'dist/windows/native-shared'
New-Item -ItemType Directory -Force $build | Out-Null
Push-Location $build
try {
    Invoke-Native "$QtRoot/bin/qmake.exe" @(
        (Join-Path $repo 'mvlink.pro'), '-spec', 'win32-g++',
        'CONFIG+=release', 'CONFIG-=debug debug_and_release', 'DESTDIR=.'
    )
    Invoke-Native "$MingwRoot/bin/mingw32-make.exe" @('-j2')
    if (Test-Path $package) { Remove-Item -Recurse -Force $package }
    New-Item -ItemType Directory -Force $package | Out-Null
    Copy-Item 'mvlink.exe' $package
    Invoke-Native "$QtRoot/bin/windeployqt.exe" @(
        '--release', '--compiler-runtime', '--no-translations',
        '--no-opengl-sw', '--no-system-d3d-compiler',
        '--dir', $package, (Join-Path $package 'mvlink.exe')
    )
    Set-Content -Path (Join-Path $package 'qt.conf') -Value "[Paths]`nPlugins = ." -Encoding ascii
    Copy-Item (Join-Path $repo 'LICENSE') $package
    Copy-Item (Join-Path $repo 'version.txt') $package
} finally {
    Pop-Location
}

if ($Test) {
    $testBuild = Join-Path $build 'tests'
    New-Item -ItemType Directory -Force $testBuild | Out-Null
    Push-Location $testBuild
    $oldPlatform = $env:QT_QPA_PLATFORM
    try {
        Invoke-Native "$QtRoot/bin/qmake.exe" @(
            (Join-Path $repo 'mvlink-test.pro'), '-spec', 'win32-g++',
            'CONFIG+=release', 'CONFIG-=debug debug_and_release', 'DESTDIR=.'
        )
        Invoke-Native "$MingwRoot/bin/mingw32-make.exe" @('-j2')
        $env:QT_QPA_PLATFORM = 'offscreen'
        Invoke-Native (Join-Path $testBuild 'mvlink-test.exe') @()
    } finally {
        $env:QT_QPA_PLATFORM = $oldPlatform
        Pop-Location
    }
    & (Join-Path $repo 'tests/test_windows_package.ps1') -Package $package
}
Write-Host "Windows package: $package"
