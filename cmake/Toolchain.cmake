include(${CMAKE_SOURCE_DIR}/cmake/Download.cmake)

# TODO: Replace with mstorsjo/llvm-mingw once
# https://github.com/llvm/llvm-project/pull/184953
# gets merged and released.
set(MONIKA_TOOLCHAIN_REPO "trungnt2910/llvm-mingw")

# CMAKE_HOST_SYSTEM_PROCESSOR is not available at this point.
if($ENV{PROCESSOR_ARCHITECTURE} MATCHES "AMD64")
    set(MONIKA_TOOLCHAIN_ARCH "x86_64")
elseif($ENV{PROCESSOR_ARCHITECTURE} MATCHES "x86")
    set(MONIKA_TOOLCHAIN_ARCH "i686")
elseif($ENV{PROCESSOR_ARCHITECTURE} MATCHES "ARM64")
    set(MONIKA_TOOLCHAIN_ARCH "aarch64")
elseif($ENV{PROCESSOR_ARCHITECTURE} MATCHES "ARM")
    set(MONIKA_TOOLCHAIN_ARCH "armv7")
else()
    # Safe fallback.
    set(MONIKA_TOOLCHAIN_ARCH "x86")
endif()

set(MONIKA_TOOLCHAIN_DIR "${CMAKE_SOURCE_DIR}/.tools")
set(MONIKA_TOOLCHAIN_BIN_PREFIX "${MONIKA_TOOLCHAIN_DIR}/bin/${CMAKE_SYSTEM_PROCESSOR}-w64-mingw32")

if(NOT EXISTS "${MONIKA_TOOLCHAIN_BIN_PREFIX}-gcc.exe")
    execute_process(
        COMMAND powershell -NoProfile -Command
                "(git ls-remote --tags                                                  `
                    --sort=-v:refname                                                   `
                    --refs https://github.com/${MONIKA_TOOLCHAIN_REPO} |                `
                        Where-Object { $_ -notmatch 'nightly' } |                       `
                        Select-Object -First 1).Split()[-1].Replace('refs/tags/', '')"
        OUTPUT_VARIABLE MONIKA_TOOLCHAIN_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    message(STATUS "lxmonika Toolchain Version: ${MONIKA_TOOLCHAIN_VERSION}")

    string(CONCAT MONIKA_TOOLCHAIN_URL
        "https://github.com/${MONIKA_TOOLCHAIN_REPO}/releases/download/"
        "${MONIKA_TOOLCHAIN_VERSION}/"
        "llvm-mingw-${MONIKA_TOOLCHAIN_VERSION}-ucrt-${MONIKA_TOOLCHAIN_ARCH}.zip"
    )

    monika_download_zip("${MONIKA_TOOLCHAIN_URL}" "${MONIKA_TOOLCHAIN_DIR}" 1)
endif()

set(CMAKE_C_COMPILER "${MONIKA_TOOLCHAIN_BIN_PREFIX}-clang.exe")
set(CMAKE_CXX_COMPILER "${MONIKA_TOOLCHAIN_BIN_PREFIX}-clang++.exe")
set(CMAKE_RC_COMPILER "${MONIKA_TOOLCHAIN_BIN_PREFIX}-windres.exe")
