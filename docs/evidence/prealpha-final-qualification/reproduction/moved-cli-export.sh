#!/bin/sh
set -eu
export SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 JUDAS_WORLD_STATE=none
export JUDAS_TEST_SCRIPT=/build/smoke.test
cd /tmp
exec xvfb-run -a /source/judas /source/game.judasproj
