cmake_minimum_required(VERSION 3.30)

project(prog)

# Based on this link
# https://github.com/antlr/antlr4/discussions/4418

set(ANTLR_TAG "4.13.2")
#para usar docker.
set(CMAKE_MODULE_PATH "/compilers/antlr4/cmake/")
set(ANTLR_EXECUTABLE "/usr/share/java/antlr-${ANTLR_TAG}-complete.jar")
set(PROG_INCLUDE_DIR "/usr/include/antlr4-runtime/")
set(PROG_LIB_DIR "/usr/lib/")
# set(CMAKE_MODULE_PATH "/home/lmcs/Documents/git/antlr4/runtime/Cpp/cmake/")
# set(ANTLR_EXECUTABLE "/home/lmcs/Apps/antlr-${ANTLR_TAG}-complete.jar")
# set(PROG_INCLUDE_DIR "/home/lmcs/Apps/antlr4-cpp/include/antlr4-runtime")
# set(PROG_LIB_DIR "/home/lmcs/Apps/antlr4-cpp/lib")

if(NOT EXISTS "${ANTLR_EXECUTABLE}")
  message(FATAL_ERROR "Couldn't find: ${ANTLR_EXECUTABLE}")
endif()
find_package(ANTLR) # busca el archivo findANTLR.cmake

message(STATUS "Found ANTLR: ${ANTLR_VERSION}")
message(STATUS "ANTLR generated sources dir: ${ANTLR_EasyRust_OUTPUT_DIR}")

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

file(GLOB SOURCES "*.cpp")
file(GLOB GFOURS "*.g4")

antlr_target(Calc ${GFOURS} LEXER PARSER VISITOR)

llvm_map_components_to_libnames(
  llvm_libs
    core
    executionengine
    object
    orcjit
    support
    targetparser
    native
)

add_executable(${PROJECT_NAME} ${SOURCES} ${ANTLR_EasyRustF_CXX_OUTPUTS})
target_link_directories(${PROJECT_NAME} PRIVATE ${PROG_LIB_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE antlr4-runtime ${llvm_libs})
target_include_directories(
  ${PROJECT_NAME} PRIVATE
  ${PROG_INCLUDE_DIR}
  ${ANTLR_EasyRustF_OUTPUT_DIR}
  ${LLVM_INCLUDE_DIRS}
)
add_definitions(${LLVM_DEFINITIONS})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
#
