find_package(GTest REQUIRED)

include(GoogleTest)

add_executable(dblusblus_tests)

target_sources(
    dblusblus_tests
    PRIVATE
        encoding_test.cpp
        smoke_test.cpp
)

target_link_libraries(
    dblusblus_tests
    PRIVATE
        dblusblus_core
        dblusblus_project_options
        dblusblus_warnings
        GTest::gtest_main
)

gtest_discover_tests(dblusblus_tests)
