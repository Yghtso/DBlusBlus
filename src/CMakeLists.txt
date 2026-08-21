add_library(dblusblus_core STATIC)

target_sources(
    dblusblus_core
    PRIVATE
        common/crc32c.cpp
        common/crc32c.h
        common/encoding.h
        common/types.h
        storage/disk/disk_manager.cpp
        storage/disk/disk_manager.h
        storage/file/file_superblock.cpp
        storage/file/file_superblock.h
        storage/file/page_file.cpp
        storage/file/page_file.h
        storage/page/page.cpp
        storage/page/page.h
        storage/page/page_header.h
        storage/heap/fsm_candidate_index.cpp
        storage/heap/fsm_candidate_index.h
        storage/heap/fsm_page.cpp
        storage/heap/fsm_page.h
        storage/heap/heap_page.cpp
        storage/heap/heap_page.h
        storage/heap/heap_page_format.cpp
        storage/heap/heap_page_format.h
        storage/tuple/tuple_codec.cpp
        storage/tuple/tuple_codec.h
        storage/tuple/tuple_header.cpp
        storage/tuple/tuple_header.h
        storage/tuple/tuple_layout.cpp
        storage/tuple/tuple_layout.h
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
