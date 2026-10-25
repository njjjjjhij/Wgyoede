cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(WClickGui VERSION 1.0.0)

# Finds main.cpp whether it is in src/ or at the top of the repo.
file(GLOB SOURCES CONFIGURE_DEPENDS src/*.cpp *.cpp)
if (NOT SOURCES)
    message(FATAL_ERROR "No .cpp file found. Put main.cpp in src/ or next to CMakeLists.txt")
endif()
add_library(${PROJECT_NAME} SHARED ${SOURCES})

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Define the GEODE_SDK environment variable.")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)
setup_geode_mod(${PROJECT_NAME})
