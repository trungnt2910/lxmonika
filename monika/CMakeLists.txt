project(monika)

monika_feature(Core)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/res)

file(GLOB_RECURSE MONIKA_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_executable(
    monika

    ${MONIKA_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/res/monika.rc
)

# Annoying. For complex structs we are not meant to init everything from the start.
target_compile_options(monika PRIVATE -Wno-missing-field-initializers)

target_link_libraries(monika ntdll)

target_link_options(monika PRIVATE -static)
