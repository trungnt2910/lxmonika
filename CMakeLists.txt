cmake_minimum_required(VERSION 3.24)
include(${CMAKE_SOURCE_DIR}/cmake/Toolchain.cmake)
project(lxmonika)

set(CMAKE_CXX_STANDARD 26)

# Catch more errors during compilation.
add_compile_options(-Werror)
add_compile_options(-Wall)
add_compile_options(-Wextra)

# Build system hacks.
include(${CMAKE_SOURCE_DIR}/cmake/Hacks.cmake)

# Get the host and target architectures.
include(${CMAKE_SOURCE_DIR}/cmake/Architecture.cmake)

# Support for features.
include(${CMAKE_SOURCE_DIR}/cmake/Feature.cmake)

# Support for sanitizers.
include(${CMAKE_SOURCE_DIR}/cmake/Sanitizers.cmake)

# Support for PDBs.
include(${CMAKE_SOURCE_DIR}/cmake/PDB.cmake)

# Support for UTF-16 resources.
include(${CMAKE_SOURCE_DIR}/cmake/Resources.cmake)

# Generate `MONIKA_` properties and add default build options.
include(${CMAKE_SOURCE_DIR}/cmake/BuildProps.cmake)

# Support for building kernel-mode drivers
include(${CMAKE_SOURCE_DIR}/cmake/KernelModeDriver.cmake)

add_subdirectory(lxstub)
add_subdirectory(lxmonika)
add_subdirectory(monika)
add_subdirectory(mxss)
add_subdirectory(packaging)

# Add targets such as FeatureCore and FeatureMXSS.
# Must always be at the very end of this file.
monika_add_feature_targets()
