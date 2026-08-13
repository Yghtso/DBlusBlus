function(dblusblus_setup_project_options)

    add_library(dblusblus_project_options INTERFACE)

    target_compile_features(
        dblusblus_project_options
        INTERFACE
            cxx_std_20
    )

    add_library(dblusblus_warnings INTERFACE)

    dblusblus_set_project_warnings(dblusblus_warnings)

    dblusblus_enable_sanitizers(dblusblus_project_options)

    dblusblus_enable_static_analyzers()

endfunction()
