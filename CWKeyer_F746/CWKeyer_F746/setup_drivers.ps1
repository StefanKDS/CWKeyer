<#
.SYNOPSIS
  Copies the ST drivers (CMSIS, HAL, BSP STM32746G-Discovery, fonts, start-up
  file) from an STM32CubeF7 firmware package into this project.

.DESCRIPTION
  The firmware package is not part of the project (license / size). Sources:
   a) STM32CubeIDE / STM32CubeMX repository, e.g.
      C:\Users\<you>\STM32Cube\Repository\STM32Cube_FW_F7_V1.17.4
      (Help > Manage embedded software packages > STM32F7 > Install)
   b) git clone --recursive https://github.com/STMicroelectronics/STM32CubeF7

.EXAMPLE
  .\setup_drivers.ps1
  .\setup_drivers.ps1 -CubeF7 "D:\st\STM32CubeF7"
#>
param([string]$CubeF7 = "")

$ErrorActionPreference = "Stop"
$proj = Split-Path -Parent $MyInvocation.MyCommand.Path

if ($CubeF7 -eq "") {
    $repo = Join-Path $env:USERPROFILE "STM32Cube\Repository"
    $cand = Get-ChildItem -Path $repo -Directory -Filter "STM32Cube_FW_F7_V*" -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending | Select-Object -First 1
    if ($null -eq $cand) {
        Write-Error "STM32CubeF7 not found in $repo. Use -CubeF7 <path>."
    }
    $CubeF7 = $cand.FullName
}
Write-Host "Using firmware package: $CubeF7"

function CopyDir($src, $dst, $filter = "*") {
    $s = Join-Path $CubeF7 $src
    if (-not (Test-Path $s)) { Write-Error "missing: $s" }
    $d = Join-Path $proj $dst
    New-Item -ItemType Directory -Force -Path $d | Out-Null
    Copy-Item -Path (Join-Path $s $filter) -Destination $d -Recurse -Force
}

function CopyFile($src, $dst) {
    $s = Join-Path $CubeF7 $src
    if (-not (Test-Path $s)) { Write-Error "missing: $s" }
    $d = Join-Path $proj $dst
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $d) | Out-Null
    Copy-Item -Path $s -Destination $d -Force
}

# CMSIS
CopyDir "Drivers\CMSIS\Include" "Drivers\CMSIS\Include" "*.h"
CopyDir "Drivers\CMSIS\Device\ST\STM32F7xx\Include" "Drivers\CMSIS\Device\ST\STM32F7xx\Include" "*.h"

# start-up file
CopyFile "Drivers\CMSIS\Device\ST\STM32F7xx\Source\Templates\gcc\startup_stm32f746xx.s" "Core\Startup\startup_stm32f746xx.s"

# HAL (all modules, without templates)
CopyDir "Drivers\STM32F7xx_HAL_Driver\Inc" "Drivers\STM32F7xx_HAL_Driver\Inc"
$halSrc = Join-Path $proj "Drivers\STM32F7xx_HAL_Driver\Src"
New-Item -ItemType Directory -Force -Path $halSrc | Out-Null
Get-ChildItem (Join-Path $CubeF7 "Drivers\STM32F7xx_HAL_Driver\Src\*.c") |
    Where-Object { $_.Name -notlike "*_template.c" } |
    Copy-Item -Destination $halSrc -Force
Get-ChildItem (Join-Path $proj "Drivers\STM32F7xx_HAL_Driver\Inc") -Recurse -Filter "*_template.h" | Remove-Item -Force

# BSP: only the parts used (NOT the BSP audio driver, it conflicts with Platform/audio_codec.c)
$bsp = "Drivers\BSP\STM32746G-Discovery"
foreach ($f in @("stm32746g_discovery", "stm32746g_discovery_lcd", "stm32746g_discovery_sdram",
                 "stm32746g_discovery_ts", "stm32746g_discovery_sd")) {
    CopyFile "$bsp\$f.c" "$bsp\$f.c"
    CopyFile "$bsp\$f.h" "$bsp\$f.h"
}
foreach ($c in @("Common", "wm8994", "ft5336", "rk043fn48h")) {
    CopyDir "Drivers\BSP\Components\$c" "Drivers\BSP\Components\$c" "*.h"
    $cs = Join-Path $CubeF7 "Drivers\BSP\Components\$c\*.c"
    if (Test-Path $cs) { Copy-Item $cs (Join-Path $proj "Drivers\BSP\Components\$c") -Force }
}

# USB device library (core + mass storage class), without templates
$usb = "Middlewares\ST\STM32_USB_Device_Library"
foreach ($d in @("Core\Inc", "Core\Src", "Class\MSC\Inc", "Class\MSC\Src")) {
    $dst = Join-Path $proj "$usb\$d"
    New-Item -ItemType Directory -Force -Path $dst | Out-Null
    Get-ChildItem (Join-Path $CubeF7 "$usb\$d\*") -File |
        Where-Object { $_.Name -notlike "*_template.*" } |
        Copy-Item -Destination $dst -Force
}

# fonts used by the BSP LCD driver (included as .c by stm32746g_discovery_lcd.c, not compiled separately)
CopyDir "Utilities\Fonts" "Utilities\Fonts" "*.*"

Write-Host ""
Write-Host "Done. Now in STM32CubeIDE: File > Import > General > Existing Projects into Workspace"
Write-Host "      select this folder, then Project > Build Project."
