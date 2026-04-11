project(lxmonika)

monika_feature(Core)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include_private)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

file(GLOB_RECURSE LXMONIKA_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_driver(
    lxmonika

    ${LXMONIKA_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/lxmonika.rc
)

target_compile_definitions(lxmonika PRIVATE MONIKA_IN_DRIVER=1)

target_compile_options(lxmonika PRIVATE -Wno-missing-field-initializers)

# RTL_CONSTANT_STRING
# UNICODE_STRING expects the number of **bytes** in its members,
# so this division is expected.
target_compile_options(lxmonika PRIVATE -Wno-sizeof-array-div)

target_link_libraries(lxmonika PRIVATE cng)
target_link_libraries(lxmonika PRIVATE wdmsec)

target_link_libraries(lxmonika PRIVATE lxstub)

monika_should_install_target(lxmonika SHOULD_INSTALL_LXMONIKA)
if(SHOULD_INSTALL_LXMONIKA)
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/" DESTINATION "Include/lxmonika")
endif()
