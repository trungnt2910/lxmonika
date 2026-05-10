add_library(
    monika_test_main
    STATIC

    main.cpp
)

target_link_libraries(monika_test_main PRIVATE GTest::gtest)
