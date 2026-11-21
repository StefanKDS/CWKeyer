#!/bin/sh
# PC simulator of the GUI. Usage: sh Tools/sim/run_sim.sh <out_dir> [fat_image]
cd "$(dirname "$0")/../.."
SRC="$(ls App/Src/*.c Lib/Src/*.c Gui/Src/*.c Gui/Src/screens/*.c)"
gcc -std=c11 -O1 -Wall -Wextra -IApp/Inc -ILib/Inc -IGui/Inc Tools/sim/sim_main.c $SRC -lm -o /tmp/cw_sim && /tmp/cw_sim "$@"
