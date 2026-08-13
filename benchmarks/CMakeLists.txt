find_package(benchmark REQUIRED)

add_executable(dblusblus_benchmarks)

target_sources(
    dblusblus_benchmarks
    PRIVATE
        benchmark_smoke.cpp
)

target_link_libraries(
    dblusblus_benchmarks
    PRIVATE
        dblusblus_core
        dblusblus_project_options
        dblusblus_warnings
        benchmark::benchmark_main
)
