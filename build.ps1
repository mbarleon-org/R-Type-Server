#!/usr/bin/env pwsh

param(
    [Alias('h')][switch] $Help,
    [Alias('d')][switch] $Debug,
    [Alias('c')][switch] $DoClean,
    [Alias('f')][switch] $DoFClean,
    [Alias('t')][switch] $Tests,
    [Alias('r')][switch] $Re
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ---------- helpers ----------
function Fail([string]$msg, [string]$hint = "") {
    Write-Host "[ERROR] $msg" -ForegroundColor Red
    if ($hint) { Write-Host "        $hint" -ForegroundColor DarkRed }
    exit 84
}
function Ok([string]$msg)   { Write-Host "[OK]    $msg" -ForegroundColor Green }
function Info([string]$msg) { Write-Host "[INFO]  $msg" -ForegroundColor Yellow }

function Assert-Tool([string]$tool) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        Fail "command '$tool' not found" "please install '$tool'"
    }
}
function Ensure-Tools() {
    @('git','cmake','ninja') | ForEach-Object { Assert-Tool $_ }
    Ok "required tools found (git, cmake, ninja)"
}
function Get-CpuCount() {
    if ($env:NUMBER_OF_PROCESSORS) { return [int]$env:NUMBER_OF_PROCESSORS }
    return 4
}

# ---------- core actions ----------
function Invoke-ConfigureAndBuild([string]$BuildType, [string[]]$ExtraFlags) {
    Ensure-Tools
    Info "updating external submodules..."
    git submodule update --init --recursive
    Ok "updated external submodules"

    if (-not (Test-Path ./build)) { New-Item -ItemType Directory -Path ./build | Out-Null }
    Push-Location ./build
    try {
        Info "configuring with CMake (Ninja, $BuildType)"
        $cmakeArgs = @("..","-G","Ninja","-DCMAKE_BUILD_TYPE=$BuildType") + $ExtraFlags
        & cmake @cmakeArgs | Out-Host

        Info "building target r-type_server with Ninja"
        & ninja -j (Get-CpuCount) r-type_server | Out-Host

        Ok "compiled r-type_server"
    } catch {
        Fail "compilation error" "failed to compile r-type_server"
    } finally {
        Pop-Location
    }
}

function Invoke-BuildRelease() { Invoke-ConfigureAndBuild -BuildType "Release" -ExtraFlags @() }
function Invoke-BuildDebug()   { Invoke-ConfigureAndBuild -BuildType "Debug"   -ExtraFlags @("-DENABLE_DEBUG=ON") }

function Invoke-RunTests() {
    Ensure-Tools
    if (-not (Test-Path ./build)) { New-Item -ItemType Directory -Path ./build | Out-Null }
    Push-Location ./build
    try {
        Info "configuring tests with CMake (Ninja, Debug)"
        & cmake ".." -G "Ninja" -DCMAKE_BUILD_TYPE=Debug | Out-Host

        Info "building unit tests (zap_srv_unit_tests)"
        & ninja -j (Get-CpuCount) zap_srv_unit_tests | Out-Host
    } catch {
        Fail "unit tests compilation error" "failed to compile zap_srv_unit_tests"
    } finally {
        Pop-Location
    }

    $testExe = Join-Path "build" "zap_srv_unit_tests.exe"
    if (-not (Test-Path $testExe)) {
        $alt = Join-Path "build" "zap_srv_unit_tests"
        if (Test-Path $alt) { $testExe = $alt }
    }

    Info "running unit tests"
    & "$testExe"
    Ok "unit tests succeed"

    $coverageOut = "code_coverage.txt"
    try {
        if (Get-Command gcovr -ErrorAction SilentlyContinue) {
            Info "generating coverage (gcovr)"
            gcovr -r . --exclude tests/ | Out-File -Encoding utf8 $coverageOut
        } else {
            "Coverage not available on this runner." | Out-File -Encoding utf8 $coverageOut
        }
    } catch {
        "Coverage step failed or unavailable." | Out-File -Encoding utf8 $coverageOut
    }
    Get-Content $coverageOut | Write-Host
}

function Invoke-Clean() {
    Remove-Item -Recurse -Force -ErrorAction SilentlyContinue build
}

function Invoke-FClean() {
    Invoke-Clean
    $paths = @(
        "r-type_ecs",
        "*.so","*.dylib", "*.dll","*.lib","*.a","*.exp",
        "rtype_srv_unit_tests","rtype_srv_unit_tests.exe","r-type_server",
        "r-type_server.exe","unit_tests","plugins","code_coverage.txt",
        "unit_tests-*.profraw","unit_tests.profdata","vgcore*","cmake-build-debug"
    )
    foreach ($p in $paths) { Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $p }
    if (Test-Path libs) { Get-ChildItem libs | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue }
}

if ($Help) {
@"
USAGE:
  .\build.ps1       builds r-type_server project (Ninja)

ARGUMENTS:
  .\build.ps1 [-h]        displays this message
  .\build.ps1 [-d]        debug flags compilation
  .\build.ps1 [-c]        clean the project
  .\build.ps1 [-f]        fclean the project
  .\build.ps1 [-t]        run unit tests
  .\build.ps1 [-r]        fclean then rebuild (release)
"@
    exit 0
}

if     ($DoClean)  { Invoke-Clean;  exit 0 }
elseif ($DoFClean) { Invoke-FClean; exit 0 }
elseif ($Debug)    { Invoke-BuildDebug; exit 0 }
elseif ($Tests)    { Invoke-RunTests;   exit 0 }
elseif ($Re)       { Invoke-FClean; Invoke-BuildRelease; exit 0 }
else               { Invoke-BuildRelease }
