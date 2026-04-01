#!/bin/bash

docker run -it --rm \
-v $(pwd):/app \
-w /app \
cpp-dev \
sh -c "
cmake -S . -B build &&
find src -name '*.cpp' -o -name '*.hpp' | entr -r sh -c '
    echo \"🔨 Building...\"
    cmake --build build &&
    echo \"🚀 Running...\"
    ./build/game_engine
'
"