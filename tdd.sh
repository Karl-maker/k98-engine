#!/bin/bash

docker run -it --rm \
-v $(pwd):/app \
-w /app \
cpp-dev \
sh -c "
cmake -S . -B build &&
find . -name '*.cpp' -o -name '*.h' | entr -r sh -c 'cmake --build build && ctest --test-dir build --output-on-failure'
"