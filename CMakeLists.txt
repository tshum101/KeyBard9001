cmake_minimum_required(VERSION 3.22)

# The firmware project is 00_DesignFiles/CMakeLists.txt.
# This file only marks the repo root as a CMake project so the STM32Cube
# extension allows this folder in KeyBard9001.code-workspace. Do not add
# targets here, and do not point STM32CubeMX at this file.
project(KeyBard9001Repo NONE)
