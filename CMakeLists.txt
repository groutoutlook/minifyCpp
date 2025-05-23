cmake_minimum_required(VERSION 3.22.1)
project(minifyCpp VERSION 1.2.1)

# configuration
#set(LLVMVersion "21" CACHE STRING "Which LLVM and Clang version to use")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_FLAGS "-fno-rtti")

find_package(LLVM REQUIRED)
find_package(Clang REQUIRED)

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CLANG_INCLUDE_DIRS})
include_directories(include)

add_definitions(${LLVM_DEFINITIONS})
add_definitions(${CLANG_DEFINITIONS})

include(AddLLVM)
set(CMAKE_MODULE_PATH "D:/ProgramDataD/MiscLang/01-C++/repo/llvm-project/clang/cmake/modules" "D:/ProgramDataD/MiscLang/01-C++/repo/llvm-project/llvm/cmake/modules")
include(AddClang)

macro (add_tooling_executable name)
  add_clang_executable(${name} ${ARGN})
  target_link_libraries(
    ${name}  
    PRIVATE
    clangAST
    clangBasic
    clangFrontend
    clangSerialization
    clangTooling
  )
  install(
    TARGETS
    ${name}
    RUNTIME
    DESTINATION
    bin
  )
endmacro(add_tooling_executable)

# debug executable, for development
if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
  add_tooling_executable(explorer src/explorer.cpp)
  add_tooling_executable(ciexplorer src/ciexplorer.cpp)
endif()

# actual applications
add_tooling_executable(
  minifier
  # MAIN FILE
  src/main.cpp

  # ACTIONS
  src/actions/AddDefinesAction.cpp
  src/actions/ExpandMacroAction.cpp
  src/actions/FormatAction.cpp
  src/actions/MinifySymbolsAction.cpp
  src/actions/PPSymbolsAction.cpp

  # UTILS
  src/util/symbols.cpp
)

# package
set(LLVMDEP "libllvm${LLVMVersion}")
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "golfC")
set(CPACK_PACKAGE_VENDOR "chrehall68")
set(CPACK_PACK_DEBIAN_ARCHITECTURE ${CMAKE_HOST_SYSTEM_PROCESSOR})
set(CPACK_DEBIAN_PACKAGE_DEPENDS ${LLVMDEP})
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "chrehall68")
include(CPack)