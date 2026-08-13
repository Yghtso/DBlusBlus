function(dblusblus_set_project_warnings target_name)

    option(
        DBLUSBLUS_WARNINGS_AS_ERRORS
        "Treat compiler warnings as errors"
        OFF
    )

    set(CLANG_WARNINGS
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wshadow
        -Wformat=2
        -Wundef
        -Wcast-align
        -Wnon-virtual-dtor
        -Woverloaded-virtual
    )

    set(GCC_WARNINGS
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wshadow
        -Wformat=2
        -Wundef
        -Wcast-align
        -Wnon-virtual-dtor
        -Woverloaded-virtual
    )

    if(DBLUSBLUS_WARNINGS_AS_ERRORS)
        list(APPEND CLANG_WARNINGS -Werror)
        list(APPEND GCC_WARNINGS -Werror)
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(
            ${target_name}
            INTERFACE
                ${CLANG_WARNINGS}
        )
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(
            ${target_name}
            INTERFACE
                ${GCC_WARNINGS}
        )
    endif()

endfunction()
