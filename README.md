# NormalToHeight [![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)

## Description
A small program to generate height maps for normal maps. Based on the algorithm described on page 16 of this 2018 paper/presenation by Danny Chan https://advances.realtimerendering.com/s2018/MaterialAdvancesInWWII-course_notes.pdf

## Prerequisites
### Operating Systems and Compilers
This engine requires at least C++ 20, and has only been tested on the following platforms and compilers:
- Windows 11 with MSVC 2022

## Compiling Windows
From git bash:
```
git clone https://github.com/LiamTyler/NormalToHeight.git
cd NormalToHeight 
mkdir build
cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
```
Then either open the Visual Studio .sln file and build there (ctrl-shift-B) or you can build same git bash command line with `cmake --build . --config [Debug/Release]`

## Compiling Linux
From the command line:
```
git clone --recursive https://github.com/LiamTyler/NormalToHeight.git
cd NormalToHeight 
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=[Debug/Release/Ship] ..
make -j
```

## Usage
TODO (no command line interface yet, just hard coded paths and such)