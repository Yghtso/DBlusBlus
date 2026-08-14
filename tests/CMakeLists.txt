find_package(GTest REQUIRED)

include(GoogleTest)

add_executable(dblusblus_tests)

target_sources(
    dblusblus_tests
    PRIVATE
        crc32c_test.cpp
        disk_manager_test.cpp
        encoding_test.cpp
        file_superblock_test.cpp
        heap_page_test.cpp
        page_header_test.cpp
        page_file_test.cpp
        page_test.cpp
        smoke_test.cpp
        tuple_header_test.cpp
        tuple_layout_test.cpp
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
