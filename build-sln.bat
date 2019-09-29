@echo off

rem cmake -S . -B ./build/win64 -G "Visual Studio 15 2017 Win64"
rem cmake --build ./build/win64

./build-cmake.bat vs2017 build %1
