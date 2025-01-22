project(GolfC)
cmake_minimum_required(VERSION 3.5.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-fno-rtti")

macro(add_clang_executable name)
    add_executable(${name} ${ARGN})
    target_include_directories(${name} PRIVATE include)

    target_link_libraries(${name}
        PRIVATE
        LLVMSupport
        clangAST
        clangBasic
        clangFrontend
        clangSerialization
        clangTooling
        clangFrontend
        clangParse
        clangSerialization
        clangSema
        clangAPINotes
        clangEdit
        clangAnalysis
        clangSupport
        LLVMFrontendHLSL
        clangDriver
        LLVMWindowsDriver
        LLVMOption
        clangASTMatchers
        clangAST
        clangFormat
        clangToolingInclusions
        clangToolingCore
        clangRewrite
        clangLex
        clangBasic
        LLVMFrontendOpenMP
        LLVMScalarOpts
        LLVMAggressiveInstCombine
        LLVMInstCombine
        LLVMFrontendOffloading
        LLVMTransformUtils
        LLVMFrontendAtomic
        LLVMAnalysis
        LLVMProfileData
        LLVMSymbolize
        LLVMDebugInfoPDB
        LLVMDebugInfoCodeView
        LLVMDebugInfoMSF
        LLVMDebugInfoBTF
        LLVMDebugInfoDWARF
        LLVMObject
        LLVMMCParser
        LLVMMC
        LLVMIRReader
        LLVMBitReader
        LLVMAsmParser
        LLVMCore
        LLVMRemarks
        LLVMBitstreamReader
        LLVMTextAPI
        LLVMBinaryFormat
        LLVMTargetParser
        LLVMSupport
        -lrt
        -ldl
        -lm
        libz.so
        libzstd.so
        LLVMDemangle
    )
endmacro(add_clang_executable)


add_clang_executable(explorer src/explorer.cpp)
add_clang_executable(minifier src/actions/minifier.cpp)
add_clang_executable(tokens src/main.cpp src/format/minifyFormat.cpp)
