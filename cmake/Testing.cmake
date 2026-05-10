enable_testing()

function(monika_add_mtest_executable name)
    add_executable(${name} ${ARGN})

    target_link_libraries(${name} PRIVATE GTest::gtest)
    target_link_libraries(${name} PRIVATE monika_test_main)

    gtest_discover_tests(
        ${name}

        DISCOVERY_MODE PRE_TEST
    )
endfunction()
