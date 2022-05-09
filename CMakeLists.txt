cmake_minimum_required(VERSION 3.0.0)
project(avbridge VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)

set(AVBRIDGE_DEPS_LIB "deps/lib")

find_library(LIBFFTW "fftw3f" ${AVBRIDGE_DEPS_LIB})

file(GLOB_RECURSE AVBRIDGE_SOURCES "src/*.cpp")
add_executable(avbridge ${AVBRIDGE_SOURCES})
target_link_libraries(avbridge ${LIBFFTW})


set(CMAKE_EXPORT_COMPILE_COMMANDS 1)