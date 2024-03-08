cmake_minimum_required (VERSION 3.7.2)
project(Normal2Height)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(helpful_functions)

SET_BIN_AND_LIB_DIRS(${CMAKE_BINARY_DIR}/bin ${CMAKE_BINARY_DIR}/lib)
SET_PLATFORM_DEFINES()
set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
configure_file(${ROOT_DIR}/cmake/platform_defines.hpp.in ${ROOT_DIR}/code/shared/platform_defines.hpp)

find_package(OpenMP REQUIRED)

set(SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/code/sandbox_main.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/code/image.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/code/image.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/code/image_load.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/code/image_save.cpp
)

set(EXT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/code/external)
set(SHARED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/code/shared)
set(
	EXTERNALS
	
    ${EXT_DIR}/stb/stb_image.h
    ${EXT_DIR}/stb/stb_image_write.h
    ${EXT_DIR}/stb/stb_image_resize.h
    ${EXT_DIR}/tinyexr/tinyexr.h
    ${EXT_DIR}/tinyexr/tinyexr.cc
	
	${SHARED_DIR}/assert.hpp
	${SHARED_DIR}/core_defines.hpp
    ${SHARED_DIR}/filesystem.cpp
    ${SHARED_DIR}/filesystem.hpp
    ${SHARED_DIR}/float_conversions.hpp
    ${SHARED_DIR}/logger.cpp
    ${SHARED_DIR}/logger.hpp
	${SHARED_DIR}/math.hpp
    ${SHARED_DIR}/math_base.hpp
    ${SHARED_DIR}/math_matrix.cpp
    ${SHARED_DIR}/math_matrix.hpp
    ${SHARED_DIR}/math_vec.cpp
    ${SHARED_DIR}/math_vec.hpp
    ${SHARED_DIR}/platform_defines.hpp
    ${SHARED_DIR}/time.cpp
    ${SHARED_DIR}/time.hpp
)

if(MSVC)
    if(NOT EXISTS ${CMAKE_BINARY_DIR}/lib/tiff.lib)
        CONFIG_TIME_COMPILE(${EXT_DIR}/libtiff ${CMAKE_BINARY_DIR}/ext/libtiff Release)
        COPY_BUILD_FILES(${CMAKE_BINARY_DIR}/ext/libtiff/libtiff/Release ${CMAKE_BINARY_DIR}/bin ${CMAKE_BINARY_DIR}/lib tiff)
    endif()
else()
    if(NOT EXISTS ${CMAKE_BINARY_DIR}/bin/libtiff.so)
        CONFIG_TIME_COMPILE(${EXT_DIR}/libtiff ${CMAKE_BINARY_DIR}/ext/libtiff Release)
        COPY_BUILD_FILES(${CMAKE_BINARY_DIR}/ext/libtiff/libtiff ${CMAKE_BINARY_DIR}/bin ${CMAKE_BINARY_DIR}/lib tiff)
    endif()
endif()

set(ALL_FILES ${SRC} ${EXTERNALS})
source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${ALL_FILES})
add_executable(${PROJECT_NAME} ${ALL_FILES})
SET_TARGET_COMPILE_OPTIONS_DEFAULT(${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/code/
	${CMAKE_CURRENT_SOURCE_DIR}/code/external/
	${EXT_DIR}/libtiff/libtiff/
    ${CMAKE_BINARY_DIR}/ext/libtiff/libtiff/
)

target_link_libraries(${PROJECT_NAME} PUBLIC debug OpenMP::OpenMP_CXX
	tiff
)
target_link_libraries(${PROJECT_NAME} PUBLIC optimized OpenMP::OpenMP_CXX
    tiff
)
target_link_directories(${PROJECT_NAME} PUBLIC ${CMAKE_BINARY_DIR}/lib ${CMAKE_BINARY_DIR}/bin)
