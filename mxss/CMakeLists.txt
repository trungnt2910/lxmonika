project(mxss)

monika_feature(MXSS)

set(MONIKA_MXSS_ARCH_SUPPORTED FALSE)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|aarch64")
    set(MONIKA_MXSS_ARCH_SUPPORTED TRUE)
endif()

if(MONIKA_MXSS_ARCH_SUPPORTED)
    add_subdirectory(monix)
    add_subdirectory(mxhost)
    add_subdirectory(mxss)
endif()
