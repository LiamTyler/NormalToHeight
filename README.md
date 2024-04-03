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
cmake -DCMAKE_BUILD_TYPE=[Debug/Release] ..
make -j
```

## Usage

`NormalToHeight --help`:
```
Usage: NormalToHeight [options] PATH_TO_NORMAL_MAP
Will generate height map(s) and will create and output them in a directory called
  '[PATH_TO_NORMAL_MAP]__autogen/'
Note: this tool expects the normal map to have +X to the right, and +Y down.
  See the --flipY option if the +Y direction is up

Options
  -g, --genNormalMap    Generate the normal map from the generated height map to compare to the original
  -h, --help            Print this message and exit
  -i, --iterations=N    How many iterations to use while generating the height map. Default is 1024
      --iterMultipier=X Only applicable with HeightGenMethod::RELAXATION*. The lower this is, the fewer
                          iterations happen on the largest mips. (0, 1]. Default is 0.25
  -m  --method          Which method to use to generate the height map
                          (0 == RELAXATION, 1 == RELAXTION_EDGE_AWARE, 2 == LINEAR_SYSTEM).
                          The outputted height maps will have '_gh_', '_ghe_', or '_ghl_'
                          in their postfixes, respectively.
  -r, --range           If specified, will output several images, with a
                          range of iterations (ignoring the -i command).
                        This can take a long time, especially for large images.
                          Suggested on 1024 or smaller images
  -s, --slopeScale=X    How much to scale the normals by, before generating the height map. Default is 1.0
  -w, --withoutGuess    Only applicable with HeightGenMethod::LINEAR_SYSTEM. By default,
                          it generates a height map using RELAXATION, and uses that
                          as the initial guess for the solver
  -x, --flipX           Flip the X direction on the normal map when loading it
  -y, --flipY           Flip the Y direction on the normal map when loading it
```

### Usage Examples

```
NormalToHeight.exe -g ../normal_maps/rock_wall_10_1024.png
NormalToHeight.exe -y ../normal_maps/synthetic_shapes_1_512.png
NormalToHeight.exe ../normal_maps/synthetic_rings_512.png
```

## Credits for the source normal maps:
- rock_wall_10_1k: https://polyhaven.com/a/rock_wall_10
- pine_bark_nor_dx_1k: https://polyhaven.com/a/pine_bark
- gray_rocks_nor_dx_1k: https://polyhaven.com/a/gray_rocks
- rock_wall_08_nor_dx_1k: https://polyhaven.com/a/rock_wall_08
- synthetic_rings: https://cpetry.github.io/NormalMap-Online/
- synthetic_shapes_1: https://dreamlight.com/how-to-create-normal-maps-from-photographs/