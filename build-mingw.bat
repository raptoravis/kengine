@echo off

rem cmake -S . -B ./build/mingw -G "MinGW Makefiles"

rem cd ./build/mingw
rem mingw32-make
rem cd ../..

rem cmake --build ./build/mingw

./build-cmake.bat mingw build %1