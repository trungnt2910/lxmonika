# Annoying stuff from the target file we want to override.

# These GNU-style flags mess up when we switch to lld-link mode.
string(REPLACE
    "${CMAKE_GNULD_IMAGE_VERSION}"
    ""
    CMAKE_CXX_CREATE_SHARED_LIBRARY
    "${CMAKE_CXX_CREATE_SHARED_LIBRARY}"
)

string(REPLACE
    "-Wl,--out-implib,<TARGET_IMPLIB>"
    ""
    CMAKE_CXX_CREATE_SHARED_LIBRARY
    "${CMAKE_CXX_CREATE_SHARED_LIBRARY}"
)

# Avoid unwanted user-mode libaries from messing up with kernel-mode projects.
set(CMAKE_CXX_STANDARD_LIBRARIES "")
