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
```
NormalToHeight [options] PATH_TO_NORMAL_MAP
Will generate height map(s) and will create and output them in a directory called '[PATH_TO_NORMAL_MAP]__autogen/'
Note: this tool expects the normal map to have +Y pointed down. If it's not, use the --flipY option

Options
  -f, --flipY           Flip the Y direction on the normal map when loading it
  -g, --genNormalMap    Generate the normal map from the generated height map to compare to the original
  -h, --help            Print this message and exit
  -i, --iterations=N    How many iterations to use while generating the height map. Default is 1024
  -r, --range           If specified, will output several images, with a range of iterations (ignoring the -i command).
                        This can take a long time, especially for large images. Suggested on 1024 or smaller images
  -s, --slopeScale=X    How much to scale the normals by, before generating the height map. Default is 1.0
```

Please use `NormalToHeight --help` for the latest usage


## Credits for the source normal maps:
rock_wall_10: https://polyhaven.com/a/rock_wall_10
synthetic_rings: https://cpetry.github.io/NormalMap-Online/
synthetic_shapes_1: https://dreamlight.com/how-to-create-normal-maps-from-photographs/