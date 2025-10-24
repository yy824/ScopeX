cmake_minimum_required(VERSION 3.14)

include(cmake/prelude.cmake)

project(
    scopeX
    VERSION 0.1.0
    DESCRIPTION "order matching system"
    HOMEPAGE_URL "none"
    LANGUAGES CXX
)

include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)

# ---- Declare library ----

add_library(
    scopeX_engine STATIC
    source/libs/engine/engine.cpp
)

add_library(scopeX::engine ALIAS scopeX_engine)

target_include_directories(
    scopeX_engine ${warning_guard}
    PUBLIC
      "\$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
      "\$<INSTALL_INTERFACE:include>"
)

target_compile_features(scopeX_engine PUBLIC cxx_std_20)

find_package(fmt REQUIRED)
target_link_libraries(scopeX_engine PRIVATE fmt::fmt)

# ---- Declare executable ----

add_executable(scopeX_exe source/main.cpp)
add_executable(scopeX::exe ALIAS scopeX_exe)

set_property(TARGET scopeX_exe PROPERTY OUTPUT_NAME scopeX)

target_compile_features(scopeX_exe PRIVATE cxx_std_20)

target_link_libraries(scopeX_exe PRIVATE scopeX_engine)

# ---- CLI ----
add_executable(scopeX_cli source/cli/main.cpp)
set_target_properties(scopeX_cli PROPERTIES OUTPUT_NAME scopeX_cli)
target_compile_features(scopeX_cli PRIVATE cxx_std_20)
target_link_libraries(scopeX_cli PRIVATE scopeX_engine)

# ---- CLI Bench ----
add_executable(scopeX_bench source/cli/main_bench.cpp)
set_target_properties(scopeX_bench PROPERTIES OUTPUT_NAME scopeX_bench)
target_compile_features(scopeX_bench PRIVATE cxx_std_20)
target_link_libraries(scopeX_bench PRIVATE scopeX_engine)

# ---- m2 demo ----
add_executable(scopeX_m2_demo source/cli/main_m2_demo.cpp)
target_link_libraries(scopeX_m2_demo PRIVATE scopeX_engine pthread)
target_compile_features(scopeX_m2_demo PRIVATE cxx_std_20)

# ---- Install rules ----

if(NOT CMAKE_SKIP_INSTALL_RULES)
  include(cmake/install-rules.cmake)
endif()

# ---- Testing(GoogleTest + CTest) ----
set(BUILD_TESTING OFF CACHE BOOL "Build tests")

include(CTest)
if(BUILD_TESTING)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
  )
  FetchContent_GetProperties(googletest)
  if(NOT googletest_POPULATED)
    FetchContent_Populate(googletest)
    add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR} EXCLUDE_FROM_ALL)
  endif()
  add_subdirectory(tests)
endif()

# ---- Developer mode ----

if(NOT scopeX_DEVELOPER_MODE)
  return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
  message(
      AUTHOR_WARNING
      "Developer mode is intended for developers of scopeX"
  )
endif()

include(cmake/dev-mode.cmake)
