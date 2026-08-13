function(dblusblus_enable_sanitizers target_name)

    option(
        DBLUSBLUS_ENABLE_ASAN
        "Enable AddressSanitizer"
        OFF
    )

    option(
        DBLUSBLUS_ENABLE_UBSAN
        "Enable UndefinedBehaviorSanitizer"
        OFF
    )

    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        return()
    endif()

    if(DBLUSBLUS_ENABLE_ASAN)
        target_compile_options(
            ${target_name}
            INTERFACE
                -fsanitize=address
                -fno-omit-frame-pointer
        )

        target_link_options(
            ${target_name}
            INTERFACE
                -fsanitize=address
        )
    endif()

    if(DBLUSBLUS_ENABLE_UBSAN)
        target_compile_options(
            ${target_name}
            INTERFACE
                -fsanitize=undefined
                -fno-omit-frame-pointer
        )

        target_link_options(
            ${target_name}
            INTERFACE
                -fsanitize=undefined
        )
    endif()

endfunction()
