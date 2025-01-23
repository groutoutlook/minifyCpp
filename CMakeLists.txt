project(GolfC)
cmake_minimum_required(VERSION 3.5.1)

option(LLVMVersion "LLVMVersion" 17)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-fno-rtti")

find_package(LLVM ${LLVMVersion} REQUIRED)
find_package(Clang ${LLVMVersion} REQUIRED)

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CLANG_INCLUDE_DIRS})
include_directories(include)

add_definitions(${LLVM_DEFINITIONS})
add_definitions(${CLANG_DEFINITIONS})

include(AddLLVM)
include(/usr/lib/llvm-${LLVMVersion}/lib/cmake/clang/AddClang.cmake)  # not sure why this needed the explicit path...

add_clang_executable(explorer src/explorer.cpp)
add_clang_executable(minifier src/main.cpp src/format/minifyFormat.cpp src/actions/minifyAction.cpp)
target_link_libraries(explorer
  PRIVATE
  clangAST
  clangBasic
  clangFrontend
  clangSerialization
  clangTooling
)
target_link_libraries(minifier
  PRIVATE
  clangAST
  clangBasic
  clangFrontend
  clangSerialization
  clangTooling
)