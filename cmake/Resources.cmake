# Support functions for Windows resource scripts.

# See https://github.com/llvm/llvm-project/issues/63426
set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} --codepage=65001")

# Generates a UTF-8 copy of .rc files before letting targets feed them to llvm-windres.
# This is needed because LLVM does not like UTF-16 while Visual Studio does not like UTF-8.
function(monika_generate_utf8_resources OUTPUT_VAR)
    set(SRCS_ENCODED "")

    foreach(SRC IN LISTS ARGN)
        if(IS_ABSOLUTE "${SRC}")
            set(SRC_ABS "${SRC}")
        else()
            set(SRC_ABS "${CMAKE_CURRENT_SOURCE_DIR}/${SRC}")
        endif()

        cmake_path(RELATIVE_PATH SRC_ABS
                   BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                   OUTPUT_VARIABLE SRC_REL)

        set(SRC_OUT "${CMAKE_CURRENT_BINARY_DIR}/encoded/${SRC_REL}")
        get_filename_component(SRC_OUT_DIR "${SRC_OUT}" DIRECTORY)
        file(MAKE_DIRECTORY "${SRC_OUT_DIR}")

        cmake_path(RELATIVE_PATH SRC_OUT
                   BASE_DIRECTORY "${CMAKE_BINARY_DIR}"
                   OUTPUT_VARIABLE SRC_OUT_REL)

        add_custom_command(
            OUTPUT "${SRC_OUT}"
            COMMAND powershell -NoProfile -Command
                "[IO.File]::WriteAllText(
                    '${SRC_OUT}',
                    [IO.File]::ReadAllText('${SRC_ABS}'),
                    [System.Text.UTF8Encoding]::new($false)
                )"
            DEPENDS "${SRC}"
            COMMENT "Encoding UTF-8 RC source ${SRC_OUT_REL}"
            VERBATIM
        )

        set_source_files_properties("${SRC_OUT}" PROPERTIES GENERATED TRUE)

        list(APPEND SRCS_ENCODED "${SRC_OUT}")
    endforeach()

    set(${OUTPUT_VAR} "${SRCS_ENCODED}" PARENT_SCOPE)
endfunction()

# Given a list of source files, filter out .rc files and replace them with UTF-8 copies.
function(monika_ensure_utf8_resources OUTPUT_VAR)
    set(SRCS "")
    set(RCS "")

    foreach(SRC ${ARGN})
        if(SRC MATCHES "\\.rc$")
            list(APPEND RCS "${SRC}")
        else()
            list(APPEND SRCS "${SRC}")
        endif()
    endforeach()

    monika_generate_utf8_resources(RCS_UTF8 ${RCS})
    list(APPEND SRCS "${RCS_UTF8}")

    set(${OUTPUT_VAR} "${SRCS}" PARENT_SCOPE)
endfunction()
