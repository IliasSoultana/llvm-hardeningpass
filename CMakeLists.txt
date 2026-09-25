cmake_minimum_required(VERSION 3.20)
project(HardeningAuditPass CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION} at ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

# Build as a shared-library plugin loaded by opt at runtime.
add_library(HardeningAuditPass MODULE
    HardeningAuditPass.cpp
)

# LLVM symbols are resolved at opt load time, do not require them at link time.
if(APPLE)
    target_link_options(HardeningAuditPass PRIVATE -undefined dynamic_lookup)
elseif(UNIX)
    target_link_options(HardeningAuditPass PRIVATE -Wl,--allow-shlib-undefined)
endif()

# Strip the default "lib" prefix so the file is HardeningAuditPass.so / .dylib.
set_target_properties(HardeningAuditPass PROPERTIES PREFIX "")
