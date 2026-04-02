#!/bin/bash

set -e

echo "📦 Setting up build..."

mkdir -p build
cd build

cmake ..

make

./game_engine