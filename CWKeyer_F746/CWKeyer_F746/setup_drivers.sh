#!/bin/sh
# Copies the ST drivers from an STM32CubeF7 package into this project.
# Usage: sh setup_drivers.sh /path/to/STM32Cube_FW_F7_V1.17.x   (or STM32CubeF7 git clone --recursive)
set -e
CUBE="$1"
PROJ="$(cd "$(dirname "$0")" && pwd)"
if [ -z "$CUBE" ]; then
  CUBE="$(ls -d "$HOME"/STM32Cube/Repository/STM32Cube_FW_F7_V* 2>/dev/null | sort | tail -1)"
fi
[ -d "$CUBE" ] || { echo "STM32CubeF7 not found, pass the path as argument"; exit 1; }
echo "Using $CUBE"

cpd() { mkdir -p "$PROJ/$2"; cp -R "$CUBE/$1/"$3 "$PROJ/$2/"; }

cpd Drivers/CMSIS/Include Drivers/CMSIS/Include '*.h'
cpd Drivers/CMSIS/Device/ST/STM32F7xx/Include Drivers/CMSIS/Device/ST/STM32F7xx/Include '*.h'
mkdir -p "$PROJ/Core/Startup"
cp "$CUBE/Drivers/CMSIS/Device/ST/STM32F7xx/Source/Templates/gcc/startup_stm32f746xx.s" "$PROJ/Core/Startup/"

cpd Drivers/STM32F7xx_HAL_Driver/Inc Drivers/STM32F7xx_HAL_Driver/Inc '*'
find "$PROJ/Drivers/STM32F7xx_HAL_Driver/Inc" -name '*_template.h' -delete
mkdir -p "$PROJ/Drivers/STM32F7xx_HAL_Driver/Src"
for f in "$CUBE"/Drivers/STM32F7xx_HAL_Driver/Src/*.c; do
  case "$f" in *_template.c) ;; *) cp "$f" "$PROJ/Drivers/STM32F7xx_HAL_Driver/Src/";; esac
done

B=Drivers/BSP/STM32746G-Discovery
mkdir -p "$PROJ/$B"
for f in stm32746g_discovery stm32746g_discovery_lcd stm32746g_discovery_sdram stm32746g_discovery_ts stm32746g_discovery_sd; do
  cp "$CUBE/$B/$f.c" "$CUBE/$B/$f.h" "$PROJ/$B/"
done
for c in Common wm8994 ft5336 rk043fn48h; do
  mkdir -p "$PROJ/Drivers/BSP/Components/$c"
  cp "$CUBE/Drivers/BSP/Components/$c/"*.h "$PROJ/Drivers/BSP/Components/$c/"
  ls "$CUBE/Drivers/BSP/Components/$c/"*.c >/dev/null 2>&1 && cp "$CUBE/Drivers/BSP/Components/$c/"*.c "$PROJ/Drivers/BSP/Components/$c/"
done
cpd Utilities/Fonts Utilities/Fonts '*'
echo "Done."
