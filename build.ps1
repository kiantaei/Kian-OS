#Requires -Version 5.1
[CmdletBinding()]
param([switch]$Check)

# Kian OS - build the ISO from Windows.
#
# mkarchiso only runs on Arch Linux, so a Windows build happens inside one:
#   1. Docker Desktop (Linux containers) -> official archlinux:base-devel image
#   2. WSL2 with an Arch Linux distro    -> mkarchiso straight from the repo
# If neither exists the script says exactly what to install.
#
# Result: build\out\kian-os-0.4.0-x86_64.iso (+ .sha256).
# Copy that ISO next to kian-os.vmx (the VM expects that file name).

$ErrorActionPreference = 'Stop'

$Root    = $PSScriptRoot
$Profile = Join-Path $Root 'archiso\kian'
$Work    = Join-Path $Root 'build\work'
$Out     = Join-Path $Root 'build\out'

function Say($m){ Write-Host "[kian-build] $m" -ForegroundColor Cyan }
function Die($m){ Write-Host "[kian-build] ERROR: $m" -ForegroundColor Red; exit 1 }

# ---- sanity checks ----------------------------------------------------------
if (-not (Test-Path -PathType Container $Profile)) {
    Die "profile not found: $Profile"
}
foreach ($s in @(
    'archiso/kian/profiledef.sh',
    'archiso/kian/airootfs/usr/local/bin/kian-setup',
    'archiso/kian/airootfs/usr/local/bin/kian-panel-position-fix',
    'archiso/kian/airootfs/usr/bin/xenox',
    'archiso/kian/airootfs/usr/local/share/kian-ci/verify-commands.sh')) {
    if (-not (Test-Path (Join-Path $Root $s))) { Die "missing file: $s" }
}

New-Item -ItemType Directory -Force -Path $Work, $Out | Out-Null

$buildCmd = @'
set -e
pacman -Syu --noconfirm archiso grub syslinux mtools dosfstools
mkdir -p build/work build/out
mkarchiso -v -w build/work -o build/out archiso/kian
'@

function Finish {
    $iso = Get-ChildItem -Path $Out -Filter *.iso -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $iso) { Die "build finished but no ISO was found in $Out" }
    $hash = (Get-FileHash -Algorithm SHA256 -Path $iso.FullName).Hash.ToLower()
    # GNU sha256sum-compatible format: "<hash>  <filename>"
    "$hash  $($iso.Name)" | Set-Content -Encoding ASCII -Path "$($iso.FullName).sha256"
    Say "done: $($iso.FullName)"
    Say "sha256: $hash  $($iso.Name)"
    Say "attach that ISO to the VM in kian-os.vmx (it expects the file in this folder)"
}

# ---- 1) Docker Desktop (Linux containers) -----------------------------------
if (Get-Command docker -ErrorAction SilentlyContinue) {
    Say "building in archlinux:base-devel (Docker must be running) ..."
    docker run --rm --privileged -v "${Root}:/work" -w /work `
        archlinux:base-devel bash -lc $buildCmd
    if ($LASTEXITCODE -ne 0) { Die "docker build failed (exit $LASTEXITCODE)" }
    Finish
    exit 0
}

# ---- 2) WSL with Arch Linux -------------------------------------------------
if (-not (Get-Command wsl -ErrorAction SilentlyContinue)) {
    Die @"
Neither Docker nor WSL is available, so the ISO cannot be built natively on Windows.
Pick one and re-run .\build.ps1:

  A. Install Docker Desktop (Linux containers), start it, re-run this script.
  B. Install Arch Linux in WSL:   wsl --install -d archlinux
     then inside it:              sudo pacman -S archiso grub syslinux mtools dosfstools
  C. Push to main; .github/workflows/build-iso.yml builds and publishes the ISO.
"@
}

$arch = (& wsl.exe -l -q 2>$null) |
    ForEach-Object { $_.Trim() } |
    Where-Object { $_ -match 'arch' } |
    Select-Object -First 1
if (-not $arch) {
    Die @"
No Arch Linux WSL distro found. Either:
  A. Install Docker Desktop (Linux containers) and re-run this script, or
  B. Install Arch Linux in WSL:   wsl --install -d archlinux
     then inside it:              sudo pacman -S archiso grub syslinux mtools dosfstools
"@
}

Say "checking profile scripts in '$arch' ..."
$wslProfile = (& wsl.exe -d $arch -e wslpath -a $Profile).Trim()
& wsl.exe -d $arch -e bash -n "$wslProfile/profiledef.sh"
if ($LASTEXITCODE -ne 0) { Die "profiledef.sh has syntax errors" }

if ($Check) {
    Say "syntax checks passed (no ISO built)"
    exit 0
}

if (-not (& wsl.exe -d $arch -e bash -lc 'command -v mkarchiso' 2>$null)) {
    Die "mkarchiso not found in $arch. Inside WSL: sudo pacman -S archiso grub syslinux mtools dosfstools"
}

Say "building Kian OS ISO (this downloads Arch packages; takes a while) ..."
$wslWork = (& wsl.exe -d $arch -e wslpath -a $Work).Trim()
$wslOut  = (& wsl.exe -d $arch -e wslpath -a $Out ).Trim()

& wsl.exe -d $arch -u root -e bash -lc "mkarchiso -v -w '$wslWork' -o '$wslOut' '$wslProfile'"
if ($LASTEXITCODE -ne 0) { Die "mkarchiso failed (exit $LASTEXITCODE)" }

Finish
