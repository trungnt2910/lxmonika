project(mxhost)

add_executable(
    mxhost

    mxhost.cpp
)

target_link_libraries(mxhost PRIVATE ntdll)

target_link_options(mxhost PRIVATE -static)
