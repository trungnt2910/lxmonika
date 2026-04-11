project(mxss)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/lxmonika/include)

file(GLOB_RECURSE MXSS_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_driver(
    mxss

    ${MXSS_SOURCES}
)

# Override NTDDI_VERSION
# Unlike lxmonika, which tries to be 100% compatible to support fossils like x86 WSL or Astoria,
# mxss is meant to target modern systems.
target_compile_options(mxss PRIVATE -UNTDDI_VERSION -DNTDDI_VERSION=NTDDI_WIN10_RS4)

target_compile_options(mxss PRIVATE -Wno-missing-field-initializers)
target_compile_options(mxss PRIVATE -Wno-unused-local-typedef)

# RTL_CONSTANT_STRING
# UNICODE_STRING expects the number of **bytes** in its members, so this division is expected.
# TODO: Maybe move this to KernelModeDriver.cmake?
target_compile_options(mxss PRIVATE -Wno-sizeof-array-div)

target_link_libraries(mxss PRIVATE wdmsec)

target_link_libraries(mxss PRIVATE lxmonika)
