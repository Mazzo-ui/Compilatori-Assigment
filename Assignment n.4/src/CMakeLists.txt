cmake_minimum_required(VERSION 3.13)
project(Assignment4LoopFusion LANGUAGES CXX)

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++ standard to conform to")
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

add_library(LoopFusionPass MODULE LoopFusionPass.cpp)

# Su macOS i plugin LLVM non devono avere il prefisso lib.
set_target_properties(LoopFusionPass PROPERTIES
  COMPILE_FLAGS "-fno-rtti"
  PREFIX "")

# Alcune installazioni LLVM richiedono esplicitamente queste librerie.
llvm_map_components_to_libnames(LLVM_LIBS
  Core
  Analysis
  Support
  TransformUtils
)

target_link_libraries(LoopFusionPass PRIVATE ${LLVM_LIBS})
