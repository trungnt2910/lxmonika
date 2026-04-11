set(MONIKA_BUILD_PROPS_H "${CMAKE_BINARY_DIR}/monika_build_props.h")

add_custom_target(UpdateBuildProps
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR="${CMAKE_SOURCE_DIR}"
        -DOUTPUT_FILE="${MONIKA_BUILD_PROPS_H}"
        -DMONIKA_BUILD_AUTHOR="${MONIKA_BUILD_AUTHOR}"
        -DMONIKA_BUILD_NAME="${MONIKA_BUILD_NAME}"
        -P "${CMAKE_SOURCE_DIR}/cmake/GetBuildProps.cmake"
    BYPRODUCTS "${MONIKA_BUILD_PROPS_H}"
    COMMENT "Checking Git status and updating metadata..."
)

# add_compile_options does not apply to the resource compiler.
set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} --preprocessor-arg=-include")
set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} --preprocessor-arg=\"${MONIKA_BUILD_PROPS_H}\"")

# Set timestamp separately during configure time to avoid massive rebuilds.
string(TIMESTAMP MONIKA_TIMESTAMP "%a %b %d %H:%M:%S UTC %Y" UTC)

macro(monika_target_build_props name)
    add_dependencies(${name} UpdateBuildProps)

    target_compile_options(${name} PRIVATE -include${MONIKA_BUILD_PROPS_H})
    target_compile_definitions(${name} PRIVATE MONIKA_TIMESTAMP="${MONIKA_TIMESTAMP}")
endmacro()

macro(monika_target_debug_options name)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(${name} PRIVATE -O0)
        target_compile_definitions(${name} PRIVATE _DEBUG)
    else()
        target_compile_options(${name} PRIVATE -O2)
        target_compile_definitions(${name} PRIVATE NDEBUG)
    endif()
endmacro()

macro(monika_target_unicode name)
    target_compile_options(${name} PRIVATE -municode)
    target_link_options(${name} PRIVATE -municode)
endmacro()

function(add_executable name)
    monika_ensure_utf8_resources(SRCS ${ARGN})

    _add_executable(${name} ${SRCS})

    monika_target_build_props(${name})
    monika_target_debug_options(${name})
    monika_target_mingw_pdb(${name})
    monika_target_add_sanitizers(${name})
    monika_target_unicode(${name})

    monika_should_install_target(${name} SHOULD_INSTALL)
    if(SHOULD_INSTALL)
        install(FILES "$<TARGET_FILE:${name}>" DESTINATION bin/${MONIKA_INSTALL_ARCH})
        monika_install_pdb(${name})
        monika_install_sanitizers(${name})
    endif()
endfunction()

function(add_library name)
    monika_ensure_utf8_resources(SRCS ${ARGN})

    _add_library(${name} ${SRCS})

    monika_target_build_props(${name})
    monika_target_debug_options(${name})
    monika_target_mingw_pdb(${name})
    monika_target_add_sanitizers(${name})
    monika_target_unicode(${name})

    monika_should_install_target(${name} SHOULD_INSTALL)
    if(SHOULD_INSTALL)
        install(FILES "$<TARGET_FILE:${name}>" DESTINATION bin/${MONIKA_INSTALL_ARCH})
        monika_install_pdb(${name})
    endif()
endfunction()
