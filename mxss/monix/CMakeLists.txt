project(monix)

monika_feature(MXSS)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "armv7")
    set(MONIX_ELF_TARGET "armv7-eabi")
else()
    set(MONIX_ELF_TARGET "${CMAKE_SYSTEM_PROCESSOR}-elf")
endif()

file(GLOB MONIX_ROOTFS_SOURCE_FILES "src/*")

foreach(SRC_FILE IN LISTS MONIX_ROOTFS_SOURCE_FILES)
    get_filename_component(SRC_NAME "${SRC_FILE}" NAME_WE)

    set(BIN_FILE ${CMAKE_CURRENT_BINARY_DIR}/monix-${CMAKE_SYSTEM_PROCESSOR}/${SRC_NAME})

    add_custom_command(
        OUTPUT  ${BIN_FILE}
        COMMAND ${CMAKE_CXX_COMPILER}
                    ${SRC_FILE}
                    -o ${BIN_FILE}
                    --target=${MONIX_ELF_TARGET}
                    -std=c++26 -O2
                    -Werror -Wall -Wextra -Wpedantic
                    -nostdlib -static -s
                    -ffreestanding -fuse-ld=lld
                    # Hack to allow MXSS to load this binary correctly.
                    # MXSS has a bug where it does not allow unaligned offsets.
                    -Wl,--no-rosegment
        DEPENDS ${SRC_FILE}
        COMMENT "Building Monix CXX executable ${SRC_NAME}"
        VERBATIM
    )

    add_custom_target(${SRC_NAME} ALL DEPENDS ${BIN_FILE})

    monika_should_install_target(${SRC_NAME} SHOULD_INSTALL)
    if(SHOULD_INSTALL)
        install(FILES ${BIN_FILE} DESTINATION monix-${CMAKE_SYSTEM_PROCESSOR}/bin)
    endif()
endforeach()
