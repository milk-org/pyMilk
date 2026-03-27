#!/bin/bash

git clone --depth 1 --branch framework-dev git@github.com:milk-org/milk.git ./milk-clone

./clean_milk_engine.sh

mkdir -p ProcessInfo-engine
mkdir -p Fps-engine

cp milk-clone/src/engine/libprocessinfo/* ProcessInfo-engine/
cp milk-clone/src/engine/libfps/* Fps-engine/

# There is potential... but now to get:
# - the python bindings in the right place
# - the CMakeFiles to be legible
# - the disambiguated library names
# - decide if I ship the TUI or not?

rm -rf milk-clone