[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string] $Configuration = "Release",

    [string] $BuildDir = "build",

    [int] $Jobs = 0,

    [switch] $Clean,
    [switch] $Tests,
    [switch] $RunTests,
    [switch] $BundledSpdlogFmt,
    [switch] $Gtk4,
    [switch] $Package,
    [switch] $PackageFast,
    [switch] $NoNls,
    [switch] $NoZmqRemote,
    [ValidateSet("auto", "zip", "7z", "none")]
    [string] $PackageArchiveFormat = "none",
    [string] $Msys2Root
)

$ErrorActionPreference = "Stop"

function Resolve-Msys2Root {
    param([string] $ExplicitRoot)

    $candidates = @()
    if ($ExplicitRoot) {
        $candidates += $ExplicitRoot
    }
    if ($env:MSYS2_ROOT) {
        $candidates += $env:MSYS2_ROOT
    }
    $candidates += @("C:\msys64", "C:\tools\msys64")

    foreach ($candidate in $candidates) {
        if (-not $candidate) {
            continue
        }

        $root = [System.IO.Path]::GetFullPath($candidate)
        $bash = Join-Path $root "usr\bin\bash.exe"
        $gcc = Join-Path $root "ucrt64\bin\gcc.exe"
        $cmake = Join-Path $root "ucrt64\bin\cmake.exe"
        $ninja = Join-Path $root "ucrt64\bin\ninja.exe"

        if ((Test-Path $bash) -and (Test-Path $gcc) -and (Test-Path $cmake) -and (Test-Path $ninja)) {
            return $root
        }
    }

    throw "MSYS2 UCRT64 was not found. Install MSYS2, or pass -Msys2Root C:\msys64."
}

function Test-IsInsideDirectory {
    param(
        [string] $Path,
        [string] $Directory
    )

    $fullPath = [System.IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
    $fullDirectory = [System.IO.Path]::GetFullPath($Directory).TrimEnd('\', '/')

    return $fullPath.Equals($fullDirectory, [System.StringComparison]::OrdinalIgnoreCase) -or
        $fullPath.StartsWith($fullDirectory + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)
}

$ProjectRoot = $PSScriptRoot
if (-not $ProjectRoot) {
    $ProjectRoot = (Get-Location).Path
}
$ProjectRoot = [System.IO.Path]::GetFullPath($ProjectRoot)

if ([System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDirFull = [System.IO.Path]::GetFullPath($BuildDir)
} else {
    $BuildDirFull = [System.IO.Path]::GetFullPath((Join-Path $ProjectRoot $BuildDir))
}

if (-not (Test-IsInsideDirectory -Path $BuildDirFull -Directory $ProjectRoot)) {
    throw "BuildDir must be inside the project directory: $ProjectRoot"
}

$ResolvedMsys2Root = Resolve-Msys2Root -ExplicitRoot $Msys2Root
$Bash = Join-Path $ResolvedMsys2Root "usr\bin\bash.exe"

$env:MSYSTEM = "UCRT64"
$env:CHERE_INVOKING = "1"

function ConvertTo-MsysPath {
    param([string] $Path)

    $converted = & $Bash -lc 'cygpath -u "$1"' _ $Path
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to convert path for MSYS2: $Path"
    }

    return ($converted | Select-Object -First 1).Trim()
}

if ($Jobs -le 0) {
    $Jobs = [Math]::Max(1, [int][Math]::Floor([Environment]::ProcessorCount / 2))
}

if ($RunTests) {
    $Tests = $true
}

if ($Clean -and (Test-Path $BuildDirFull)) {
    Write-Host "Removing $BuildDirFull"
    Remove-Item -LiteralPath $BuildDirFull -Recurse -Force
}

$env:CT_PROJECT_ROOT_UNIX = ConvertTo-MsysPath $ProjectRoot
$env:CT_BUILD_DIR_UNIX = ConvertTo-MsysPath $BuildDirFull
$env:CT_BUILD_TYPE = $Configuration
$env:CT_JOBS = [string] $Jobs
$env:CT_BUILD_TESTING = $(if ($Tests) { "ON" } else { "OFF" })
$env:CT_RUN_TESTS = $(if ($RunTests) { "1" } else { "0" })
$env:CT_USE_SHARED_FMT_SPDLOG = $(if ($BundledSpdlogFmt) { "OFF" } else { "ON" })
$env:CT_WITH_GTK4 = $(if ($Gtk4) { "ON" } else { "OFF" })
$env:CT_USE_NLS = $(if ($NoNls) { "OFF" } else { "ON" })
$env:CT_USE_ZMQ_REMOTE = $(if ($NoZmqRemote) { "OFF" } else { "ON" })

$bashScript = @'
set -euo pipefail

repo="$CT_PROJECT_ROOT_UNIX"
build_dir="$CT_BUILD_DIR_UNIX"

cd "$repo"

if [ -f .gitmodules ] && git submodule status | grep -q '^-'; then
  echo "Initializing missing submodules..."
  git -c http.version=HTTP/1.1 submodule update --init --recursive --depth 1 --filter=blob:none --jobs 1
fi

cmake -S "$repo" -B "$build_dir" -GNinja \
  -DCMAKE_C_COMPILER=gcc \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_BUILD_TYPE="$CT_BUILD_TYPE" \
  -DBUILD_TESTING="$CT_BUILD_TESTING" \
  -DUSE_SHARED_FMT_SPDLOG="$CT_USE_SHARED_FMT_SPDLOG" \
  -DWITH_GTK4="$CT_WITH_GTK4" \
  -DUSE_NLS="$CT_USE_NLS" \
  -DUSE_ZMQ_REMOTE="$CT_USE_ZMQ_REMOTE" \
  $([ "$CT_BUILD_TESTING" = "ON" ] && echo "-DAUTO_RUN_TESTING=OFF")

cmake --build "$build_dir" --parallel "$CT_JOBS"

if [ "$CT_RUN_TESTS" = "1" ]; then
  ctest --test-dir "$build_dir" --output-on-failure
fi

echo
echo "Built: $build_dir/cherrytree.exe"
'@

Write-Host "Using MSYS2 UCRT64 at $ResolvedMsys2Root"
Write-Host "Configuration: $Configuration"
Write-Host "Build dir: $BuildDirFull"
Write-Host "Jobs: $Jobs"

$TempScript = Join-Path ([System.IO.Path]::GetTempPath()) "cherrytree-build-$PID.sh"

try {
    Set-Content -LiteralPath $TempScript -Value $bashScript -NoNewline -Encoding ASCII
    $TempScriptUnix = ConvertTo-MsysPath $TempScript
    & $Bash -lc 'bash "$1"' _ $TempScriptUnix
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
} finally {
    Remove-Item -LiteralPath $TempScript -Force -ErrorAction SilentlyContinue
}

if ($Package -or $PackageFast) {
    $packageScript = Join-Path $ProjectRoot "package-nolatex.ps1"
    if (-not (Test-Path $packageScript)) {
        throw "Missing package script: $packageScript"
    }

    $packageArgs = @{
        BuildDir = $BuildDirFull
        SkipBuild = $true
        CleanPackage = (-not $PackageFast)
        FastUpdate = $PackageFast
        ArchiveFormat = $PackageArchiveFormat
        Msys2Root = $ResolvedMsys2Root
    }
    if ($Gtk4) {
        $packageArgs.Gtk4 = $true
    }

    Write-Host "Deploying executable into portable package..."
    & $packageScript @packageArgs
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}
