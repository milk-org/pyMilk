#!/bin/bash

git clone --depth 1 --branch framework-dev git@github.com:milk-org/milk.git ./milk-clone

./clean_milk_engine.sh

mkdir -p milkengine/ProcessInfo-engine
mkdir -p milkengine/Fps-engine

cp milk-clone/src/engine/libprocessinfo/* milkengine/ProcessInfo-engine/
cp milk-clone/src/engine/libfps/* milkengine/Fps-engine/

# There is potential... but now to get:
# - the python bindings in the right place
# - the CMakeFiles to be legible
# - the disambiguated library names
# - decide if I ship the TUI or not?

rm milkengine/Fps-engine/fps_cli_*
rm milkengine/Fps-engine/fps_standalone_data.c
rm milkengine/Fps-engine/fps_lifecycle.[ch]

cp milkengine/CMakeLists_Fps-engine_fordeploy.txt milkengine/Fps-engine/CMakeLists.txt
cp milkengine/CMakeLists_ProcessInfo-engine_fordeploy.txt milkengine/ProcessInfo-engine/CMakeLists.txt

#find milkengine/ProcessInfo-engine -name '*.[ch]' -exec sed -i 's/#include "timeutils.h"/#include "ImageStreamIO\/timeutils.h"/' {} +
find milkengine/ProcessInfo-engine -name '*.[ch]' -exec sed -i 's/#include "milkDebugTools.h"/#include "ImageStreamIO\/milkDebugTools.h"/' {} +

find milkengine/Fps-engine -name '*.[ch]' -exec sed -i 's/#include "timeutils.h"/#include "ProcessInfo-engine\/timeutils.h"/' {} +
find milkengine/Fps-engine -name '*.[ch]' -exec sed -i 's/#include "milkDebugTools.h"/#include "ImageStreamIO\/milkDebugTools.h"/' {} +
find milkengine/Fps-engine -name '*.[ch]' -exec sed -i 's/#include "processinfo_signals.h"/#include "ProcessInfo-engine\/processinfo_signals.h"/' {} +
find milkengine/Fps-engine -name '*.[ch]' -exec sed -i 's/#include "processtools.h"/#include "ProcessInfo-engine\/processtools.h"/' {} +
find milkengine/Fps-engine -name '*.[ch]' -exec sed -i 's/#include [<"]processinfo.h[">]/#include "ProcessInfo-engine\/processinfo.h"/' {} +

find milkengine/Fps-engine -name '*.[ch]' -exec sed -E -i 's/#include "fps_cli_.+\.h"//' {} +
find milkengine/Fps-engine -name '*.[ch]' -exec sed -i 's/#include "fps_lifecycle.h"//' {} +



rm -rf milk-clone
