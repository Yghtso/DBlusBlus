add_library(dblusblus_core STATIC)

target_sources(
    dblusblus_core
    PRIVATE
        common/crc32c.cpp
        common/file_superblock.cpp
        common/types.cpp
        storage/disk_manager.cpp
        storage/fsm_page.cpp
        storage/heap_page.cpp
        storage/page.cpp
        storage/page_file.cpp
        storage/tuple_codec.cpp
        storage/tuple_header.cpp
        storage/tuple_layout.cpp

    PUBLIC
        common/crc32c.h
        common/encoding.h
        common/file_superblock.h
        common/page_header.h
        common/types.h
        storage/disk_manager.h
        storage/fsm_page.h
        storage/heap_page.h
        storage/page.h
        storage/page_file.h
        storage/tuple_codec.h
        storage/tuple_header.h
        storage/tuple_layout.h
)

target_include_directories(
    dblusblus_core
    PUBLIC
        "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(
    dblusblus_core
    PUBLIC
        dblusblus_project_options

    PRIVATE
        dblusblus_warnings
)

add_executable(DBlusBlus)

target_sources(
    DBlusBlus
    PRIVATE
        main.cpp
)

target_link_libraries(
    DBlusBlus
    PRIVATE
        dblusblus_core
        dblusblus_project_options
        dblusblus_warnings
)
