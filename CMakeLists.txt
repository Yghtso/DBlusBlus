cmake_minimum_required(VERSION 3.25)

project(
    DBlusBlus
    VERSION 0.1.0
    DESCRIPTION "Performance-first relational database"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

include(ProjectOptions)
include(CompilerWarnings)
include(Sanitizers)
include(StaticAnalyzers)

dblusblus_setup_project_options()

add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

option(DBLUSBLUS_BUILD_BENCHMARKS "Build DBlusBlus benchmarks" OFF)

if(DBLUSBLUS_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
