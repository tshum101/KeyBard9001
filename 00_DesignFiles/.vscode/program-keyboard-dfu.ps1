# Build the keyboard and download it through the USB DFU bootloader.
# The chip must already be in DFU (hold BTN1, tap reset, release BTN1).
# Do not pass -e all: that would erase the bootloader and the NVM sector.
$ErrorActionPreference = "Stop"

$project = Split-Path -Parent $PSScriptRoot
$elf = Join-Path $project "build\Debug\STM32F407VGT6.elf"

function Find-Cmake {
    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }
    $candidates = @(
        "C:\ST\STM32CubeCLT_1.22.0\CMake\bin\cmake.exe",
        "C:\Program Files\CMake\bin\cmake.exe"
    )
    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return $candidate
        }
    }
    throw "cmake was not found."
}

function Find-Programmer {
    $preferred = Join-Path $env:LOCALAPPDATA "stm32cube\bundles\programmer\2.23.0\bin\STM32_Programmer_CLI.exe"
    if (Test-Path $preferred) {
        return $preferred
    }
    $root = Join-Path $env:LOCALAPPDATA "stm32cube\bundles\programmer"
    $found = Get-ChildItem $root -Recurse -Filter "STM32_Programmer_CLI.exe" -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending |
        Select-Object -First 1
    if ($found) {
        return $found.FullName
    }
    throw "STM32_Programmer_CLI.exe was not found."
}

$cmake = Find-Cmake
$cli = Find-Programmer

Write-Host "Building keyboard Debug"
Set-Location $project
& $cmake --build --preset Debug
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
if (-not (Test-Path $elf)) {
    Write-Host "Build did not produce $elf"
    exit 1
}

Write-Host "Looking for Keybard9001 DFU"
$list = & $cli -l usb 2>&1 | Out-String
Write-Host $list
if ($list -notmatch "Total number of available STM32 device in DFU mode:\s*(\d+)") {
    Write-Host "The keyboard is not in DFU mode. Hold BTN1, tap reset, release BTN1, then start this again."
    exit 1
}
$dfuCount = [int]$Matches[1]
if ($dfuCount -lt 1) {
    Write-Host "The keyboard is not in DFU mode. Hold BTN1, tap reset, release BTN1, then start this again."
    exit 1
}
# CubeProgrammer often leaves this field empty for the keyboard. A single
# unnamed device is USB1. A named device that is not the keyboard is refused.
$productName = ""
$productMatch = [regex]::Match($list, "(?m)^[ \t]*Product ID[ \t]*:[ \t]*([^\r\n]*)")
if ($productMatch.Success) {
    $productName = $productMatch.Groups[1].Value.Trim()
}
if ($productName -eq "") {
    if ($dfuCount -ne 1) {
        Write-Host "More than one DFU device is connected, and none is named Keybard9001 DFU. Nothing was programmed."
        exit 1
    }
    Write-Host "DFU product name was blank. Programming the only DFU device, USB1."
}
elseif ($productName -ne "Keybard9001 DFU") {
    Write-Host "A DFU device is connected, but it is not Keybard9001 DFU ($productName). Nothing was programmed."
    exit 1
}

Write-Host "Programming $elf"
& $cli -c port=usb1 -w $elf -v
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
Write-Host "Keyboard image is programmed at 0x08010000. Release BTN1 and tap reset. The bootloader starts the keyboard."
exit 0
