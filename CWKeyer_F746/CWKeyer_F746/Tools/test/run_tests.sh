#!/bin/sh
# Host unit tests (gcc). Usage: sh Tools/test/run_tests.sh [fat images...]
cd "$(dirname "$0")/../.."
gcc -std=c11 -O1 -Wall -Wextra -IApp/Inc -ILib/Inc Tools/test/test_core.c \
    App/Src/morse.c App/Src/morse_timing.c App/Src/keyer.c App/Src/player.c \
    App/Src/cw_decoder.c App/Src/settings.c App/Src/text_source.c Lib/Src/fat_ro.c \
    -lm -o /tmp/cw_test && /tmp/cw_test "$@"
