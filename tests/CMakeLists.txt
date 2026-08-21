find_package(GTest REQUIRED)

include(GoogleTest)

add_executable(dblusblus_tests)

target_sources(
    dblusblus_tests
    PRIVATE
        common/crc32c_test.cpp
        common/encoding_test.cpp
        common/types_test.cpp
        smoke_test.cpp
        storage/disk/disk_manager_test.cpp
        storage/file/file_superblock_test.cpp
        storage/file/page_file_test.cpp
        storage/heap/fsm_candidate_index_test.cpp
        storage/heap/fsm_page_test.cpp
        storage/heap/heap_page_format_test.cpp
        storage/heap/heap_page_insert_test.cpp
        storage/heap/heap_page_persistence_test.cpp
        storage/heap/heap_page_reclamation_test.cpp
        storage/heap/heap_page_validation_test.cpp
        storage/page/page_header_test.cpp
        storage/page/page_test.cpp
        storage/tuple/tuple_codec_property_test.cpp
        storage/tuple/tuple_codec_test.cpp
        storage/tuple/tuple_header_test.cpp
        storage/tuple/tuple_layout_test.cpp
        storage/tuple/tuple_varlen_codec_test.cpp
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
