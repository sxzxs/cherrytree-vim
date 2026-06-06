[CmdletBinding()]
param(
    [string] $BuildDir = "build",

    [int] $Jobs = 0,

    [switch] $SkipBuild,
    [switch] $CleanPackage,
    [switch] $FastUpdate,
    [switch] $Gtk4,

    [ValidateSet("auto", "zip", "7z", "none")]
    [string] $ArchiveFormat = "auto",

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

if (-not $SkipBuild) {
    $buildScript = Join-Path $ProjectRoot "build.ps1"
    if (-not (Test-Path $buildScript)) {
        throw "Missing build script: $buildScript"
    }

    $buildArgs = @{
        Configuration = "Release"
        BuildDir = $BuildDirFull
        Msys2Root = $ResolvedMsys2Root
    }
    if ($Jobs -gt 0) {
        $buildArgs.Jobs = $Jobs
    }
    if ($Gtk4) {
        $buildArgs.Gtk4 = $true
    }

    Write-Host "Building release executable..."
    & $buildScript @buildArgs
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

$env:CT_PROJECT_ROOT_UNIX = ConvertTo-MsysPath $ProjectRoot
$env:CT_BUILD_DIR_UNIX = ConvertTo-MsysPath $BuildDirFull
$env:CT_ARCHIVE_FORMAT = $ArchiveFormat
$env:CT_CLEAN_PACKAGE = $(if ($CleanPackage) { "1" } else { "0" })
$env:CT_FAST_UPDATE = $(if ($FastUpdate) { "1" } else { "0" })
$env:CT_WITH_GTK4 = $(if ($Gtk4) { "ON" } else { "OFF" })

$bashScript = @'
set -euo pipefail

repo="$CT_PROJECT_ROOT_UNIX"
build_dir="$CT_BUILD_DIR_UNIX"
archive_format="$CT_ARCHIVE_FORMAT"
clean_package="$CT_CLEAN_PACKAGE"
fast_update="$CT_FAST_UPDATE"
with_gtk4="$CT_WITH_GTK4"

cd "$repo"

exe="$build_dir/cherrytree.exe"
config_h="$repo/config.h"

if [ ! -f "$exe" ]; then
  echo "missing executable: $exe" >&2
  exit 1
fi
if [ ! -f "$config_h" ]; then
  echo "missing config.h; build first" >&2
  exit 1
fi

version="$(grep PACKAGE_VERSION_WINDOWS_STR "$config_h" | awk '{print substr($3, 2, length($3)-2)}')"
if [ -z "$version" ]; then
  echo "failed to read PACKAGE_VERSION_WINDOWS_STR from config.h" >&2
  exit 1
fi

package_variant="nolatex"
gtk_share_tree="/ucrt64/share/gtk-3.0"
gtksource_share_tree="/ucrt64/share/gtksourceview-4"
if [ "$with_gtk4" = "ON" ]; then
  package_variant="gtk4_nolatex"
  gtk_share_tree="/ucrt64/share/gtk-4.0"
  gtksource_share_tree="/ucrt64/share/gtksourceview-5"
fi

out_root="$build_dir/cherrytree_${version}_win64_portable_${package_variant}"
out_ucrt64="$out_root/ucrt64"
if [ "$with_gtk4" = "ON" ]; then
  out_gtk_etc="$out_root/etc/gtk-4.0"
else
  out_gtk_etc="$out_root/etc/gtk-3.0"
fi
out_share="$out_ucrt64/usr/share/cherrytree"
out_locale="$out_ucrt64/share/locale"
out_hunspell="$out_ucrt64/share/hunspell"

if [ "$fast_update" = "1" ] && [ "$clean_package" != "1" ] && [ -d "$out_ucrt64/bin" ]; then
  echo "Fast updating existing portable package executable..."
  cp -a "$exe" "$out_ucrt64/bin/cherrytree.exe"
  strip "$out_ucrt64/bin/cherrytree.exe" 2>/dev/null || true
  cp -a "$repo/license.txt" "$out_root/" 2>/dev/null || true
  rm -f "$out_root.7z" "$out_root.zip"
  echo
  echo "Portable directory: $out_root"
  echo "Updated executable: $out_ucrt64/bin/cherrytree.exe"
  exit 0
fi

if [ "$clean_package" = "1" ] || [ -d "$out_root" ]; then
  echo "Cleaning previous no-latex portable package..."
  rm -rf "$out_root" "$out_root.7z" "$out_root.zip"
fi

mkdir -p "$out_ucrt64"

declare -A queued_binaries
binary_queue=()

copy_ucrt_file() {
  local src="$1"
  local dest="$out_root$src"

  [ -f "$src" ] || return 0
  mkdir -p "$(dirname "$dest")"
  cp -a "$src" "$dest"
}

copy_ucrt_tree() {
  local src="$1"
  local dest="$out_root$src"

  [ -e "$src" ] || return 0
  rm -rf "$dest"
  mkdir -p "$(dirname "$dest")"
  cp -a "$src" "$dest"
}

queue_binary_for_scan() {
  local src="$1"
  local key

  [ -f "$src" ] || return 0
  key="$(readlink -f "$src")"
  if [ -z "${queued_binaries[$key]+x}" ]; then
    queued_binaries[$key]=1
    binary_queue+=("$src")
  fi
}

copy_ucrt_binary_and_queue() {
  local src="$1"

  [ -f "$src" ] || return 0
  copy_ucrt_file "$src"
  queue_binary_for_scan "$src"
}

copy_dependency_from_ldd_path() {
  local dep="$1"
  local dep_name

  [ -n "$dep" ] || return 0
  dep_name="$(basename "$dep")"

  case "$dep" in
    /ucrt64/*)
      copy_ucrt_binary_and_queue "$dep"
      ;;
    /c/WINDOWS/*|/c/Windows/*|/c/windows/*)
      # If a dependency name exists in UCRT64 too, ship that build instead of
      # relying on a matching DLL in System32. This matters for libssl/libcrypto.
      if [ -f "/ucrt64/bin/$dep_name" ]; then
        copy_ucrt_binary_and_queue "/ucrt64/bin/$dep_name"
      fi
      ;;
  esac
}

echo "Copying CherryTree application files..."
mkdir -p "$out_ucrt64/bin"
cp -a "$exe" "$out_ucrt64/bin/cherrytree.exe"
cp -a "$repo/license.txt" "$out_root/"
queue_binary_for_scan "$exe"

for helper_exe in \
  /ucrt64/bin/dbus-daemon.exe \
  /ucrt64/bin/dbus-launch.exe \
  /ucrt64/bin/gdbus.exe \
  /ucrt64/bin/gio.exe \
  /ucrt64/bin/gsettings.exe \
  /ucrt64/bin/gspawn-win64-helper.exe \
  /ucrt64/bin/gspawn-win64-helper-console.exe
do
  copy_ucrt_binary_and_queue "$helper_exe"
done

echo "Copying GTK module directories..."
runtime_dirs=(
  /ucrt64/lib/gdk-pixbuf-2.0
  /ucrt64/lib/enchant-2
  /ucrt64/lib/gio/modules
)
if [ "$with_gtk4" = "ON" ]; then
  runtime_dirs+=(/ucrt64/lib/gtk-4.0)
else
  runtime_dirs+=(
    /ucrt64/lib/gtk-3.0/3.0.0/immodules
    /ucrt64/lib/gtk-3.0/3.0.0/printbackends
    /ucrt64/lib/gtk-3.0/3.0.0/engines
  )
fi

for runtime_dir in "${runtime_dirs[@]}"; do
  if [ -d "$runtime_dir" ]; then
    copy_ucrt_tree "$runtime_dir"
    while IFS= read -r -d '' module_dll; do
      queue_binary_for_scan "$module_dll"
    done < <(find "$runtime_dir" -name "*.dll" -print0)
  fi
done

echo "Scanning DLL dependencies..."
queue_index=0
while [ "$queue_index" -lt "${#binary_queue[@]}" ]; do
  current_binary="${binary_queue[$queue_index]}"
  queue_index=$((queue_index + 1))

  while IFS= read -r dep; do
    copy_dependency_from_ldd_path "$dep"
  done < <(
    ldd "$current_binary" 2>/dev/null |
      sed -n \
        -e 's/.*=> \([^ ]*\.dll\) (.*/\1/p' \
        -e 's/^[[:space:]]*\([^ ]*\.dll\) (.*/\1/p'
  )
done

echo "Copying GTK/GLib runtime resources..."
runtime_trees=(
  /ucrt64/lib/aspell-0.60
  /ucrt64/etc/fonts
  /ucrt64/etc/dbus-1
  /ucrt64/etc/ssl
  /ucrt64/share/dbus-1
  /ucrt64/share/fontconfig
  /ucrt64/share/fonts
  /ucrt64/share/enchant-2
  /ucrt64/share/glib-2.0/schemas
  "$gtk_share_tree"
  "$gtksource_share_tree"
  /ucrt64/share/icons
  /ucrt64/share/mime
  /ucrt64/share/themes
  /ucrt64/share/zoneinfo
)

for runtime_tree in "${runtime_trees[@]}"; do
  [ -e "$runtime_tree" ] && copy_ucrt_tree "$runtime_tree"
done

echo "Writing portable GTK settings..."
mkdir -p "$out_gtk_etc"
{
  echo "[Settings]"
  echo "gtk-theme-name=win32"
} > "$out_gtk_etc/settings.ini"

mkdir -p "$out_share/data"
cp -a "$repo/language-specs" "$out_share/"
cp -a "$repo/styles" "$out_share/"
cp -a "$repo/data/script3.js" "$out_share/data/"
cp -a "$repo/data/styles4.css" "$out_share/data/"
cp -a "$repo/data/user-style.xml" "$out_share/data/"

mkdir -p "$out_share/icons"
cp -a "$repo/icons/ct_home.svg" "$out_share/icons/"
cp -a "$repo/icons/Breeze_Dark_icons" "$out_share/icons/"
cp -a "$repo/icons/Breeze_Light_icons" "$out_share/icons/"

mkdir -p "$out_locale"
for lang_dir in "$repo"/po/*; do
  if [ -d "$lang_dir" ]; then
    lang_name="$(basename "$lang_dir")"
    [ -d "/ucrt64/share/locale/$lang_name" ] && copy_ucrt_tree "/ucrt64/share/locale/$lang_name"
    cp -a "$lang_dir" "$out_locale/"
  fi
done

mkdir -p "$out_hunspell"
cp -a "$repo"/hunspell/*.aff "$out_hunspell/"
cp -a "$repo"/hunspell/*.dic "$out_hunspell/"

echo "Pruning development-only package artifacts..."
find "$out_ucrt64" -name "*.a" -exec rm -f {} \;
rm -f "$out_ucrt64/lib/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-avif.dll"
rm -f "$out_ucrt64/lib/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-heif.dll"
loaders_cache="$out_ucrt64/lib/gdk-pixbuf-2.0/2.10.0/loaders.cache"
if [ -f "$loaders_cache" ]; then
  awk 'BEGIN { RS=""; ORS="\n\n" } !/libpixbufloader-(avif|heif)\.dll/' "$loaders_cache" > "$loaders_cache.tmp"
  mv "$loaders_cache.tmp" "$loaders_cache"
fi
rm -f "$out_ucrt64/share/fontconfig/conf.avail/09-texlive-fonts.conf"
rm -f "$out_ucrt64/bin/libavif-"*.dll
rm -f "$out_ucrt64/bin/libheif.dll"
rm -f "$out_ucrt64/bin/libaom.dll"
rm -f "$out_ucrt64/bin/libdav1d-"*.dll
rm -f "$out_ucrt64/bin/libde265-"*.dll
rm -f "$out_ucrt64/bin/libx265-"*.dll
rm -f "$out_ucrt64/bin/libSvtAv1Enc-"*.dll
rm -f "$out_ucrt64/bin/librav1e.dll"
rm -f "$out_ucrt64/bin/libyuv.dll"
rm -f "$out_ucrt64/bin/libopenjp2-"*.dll
rm -f "$out_ucrt64/bin/libopenjph-"*.dll
rm -f "$out_ucrt64/bin/libopenh264-"*.dll
rm -f "$out_ucrt64/bin/libkvazaar-"*.dll
rm -f "$out_ucrt64/bin/libcryptopp.dll"
if [ "$with_gtk4" = "ON" ]; then
  allowed_locale_mo='^(aspell|at-spi2-core|cherrytree|fontconfig|fontconfig-conf|gdk-pixbuf|gettext-runtime|glib20|gnutls|gtk40|gtk40-properties|gtksourceview-5|json-glib-1\.0|libidn2|libspelling|shared-mime-info|tre|xz)\.mo$'
else
  allowed_locale_mo='^(aspell|at-spi2-core|cherrytree|fontconfig|fontconfig-conf|gdk-pixbuf|gettext-runtime|glib20|gnutls|gspell-1|gtk30|gtk30-properties|gtksourceview-4|json-glib-1\.0|libidn2|shared-mime-info|tre|xz)\.mo$'
fi
while IFS= read -r -d '' mo_file; do
  mo_name="$(basename "$mo_file")"
  if [[ ! "$mo_name" =~ $allowed_locale_mo ]]; then
    rm -f "$mo_file"
  fi
done < <(find "$out_locale" -name "*.mo" -print0)

echo "Stripping packaged binaries..."
while IFS= read -r -d '' binary; do
  strip "$binary" 2>/dev/null || true
done < <(find "$out_ucrt64" \( -name "*.dll" -o -name "*.exe" \) -print0)

case "$archive_format" in
  none)
    echo "Archive disabled."
    ;;
  7z)
    seven_zip="$(command -v 7za || command -v 7z || true)"
    if [ -n "$seven_zip" ]; then
      (cd "$build_dir" && "$seven_zip" a -t7z "$(basename "$out_root").7z" "$(basename "$out_root")")
    else
      (cd "$build_dir" && bsdtar --format 7zip -cf "$(basename "$out_root").7z" "$(basename "$out_root")")
    fi
    ;;
  zip)
    (cd "$build_dir" && bsdtar --format zip -cf "$(basename "$out_root").zip" "$(basename "$out_root")")
    ;;
  auto)
    seven_zip="$(command -v 7za || command -v 7z || true)"
    if [ -n "$seven_zip" ]; then
      (cd "$build_dir" && "$seven_zip" a -t7z "$(basename "$out_root").7z" "$(basename "$out_root")")
    else
      echo "7za/7z not found; creating .7z with bsdtar."
      (cd "$build_dir" && bsdtar --format 7zip -cf "$(basename "$out_root").7z" "$(basename "$out_root")")
    fi
    ;;
esac

echo
echo "Portable directory: $out_root"
if [ -f "$out_root.7z" ]; then
  echo "Archive: $out_root.7z"
fi
if [ -f "$out_root.zip" ]; then
  echo "Archive: $out_root.zip"
fi
'@

Write-Host "Using MSYS2 UCRT64 at $ResolvedMsys2Root"
Write-Host "Packaging no-latex portable build..."

$TempScript = Join-Path ([System.IO.Path]::GetTempPath()) "cherrytree-package-nolatex-$PID.sh"

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
