project(lxstub)

monika_feature(Core)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

file(GLOB_RECURSE LXSTUB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_driver(
    lxstub

    ${LXSTUB_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/lxstub.rc
    ${CMAKE_CURRENT_SOURCE_DIR}/lxstub.def
)

set_target_properties(
    lxstub PROPERTIES

    OUTPUT_NAME "LXCORE"
)
