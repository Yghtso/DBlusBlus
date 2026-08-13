function(dblusblus_enable_static_analyzers)

    option(
        DBLUSBLUS_ENABLE_CLANG_TIDY
        "Enable clang-tidy during compilation"
        OFF
    )

    if(DBLUSBLUS_ENABLE_CLANG_TIDY)

        find_program(
            CLANG_TIDY_EXE
            NAMES clang-tidy
        )

        if(NOT CLANG_TIDY_EXE)
            message(FATAL_ERROR "clang-tidy requested but not found")
        endif()

        set(
            CMAKE_CXX_CLANG_TIDY
            "${CLANG_TIDY_EXE}"
            PARENT_SCOPE
        )

    endif()

endfunction()
