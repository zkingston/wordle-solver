cmake_minimum_required(VERSION 3.1...3.21)
project(wordle-solver VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g ${CMAKE_CXX_FLAGS_DEBUG}")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native ${CMAKE_CXX_FLAGS_RELEASE}")

set(CMAKE_CXX_FLAGS
  "-W -Wall -Wextra -Wconversion -Wcast-qual -Wwrite-strings -Wunreachable-code -Wpointer-arith -Winit-self -Wredundant-decls -Wno-unused-parameter -Wno-unused-function")

set(LINK_WHAT_YOU_USE ON)

add_executable(wordle wordle.cpp)
target_link_libraries(wordle)
