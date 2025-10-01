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
    scopeX_lib OBJECT
    source/libs/engine/engine.cpp
)

target_include_directories(
    scopeX_lib ${warning_guard}
    PUBLIC
    "\$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/source/libs/engine>"
)

target_compile_features(scopeX_lib PUBLIC cxx_std_20)

find_package(fmt REQUIRED)
target_link_libraries(scopeX_lib PRIVATE fmt::fmt)

# ---- Declare executable ----

add_executable(scopeX_exe source/main.cpp)
add_executable(scopeX::exe ALIAS scopeX_exe)

set_property(TARGET scopeX_exe PROPERTY OUTPUT_NAME scopeX)

target_compile_features(scopeX_exe PRIVATE cxx_std_20)

target_link_libraries(scopeX_exe PRIVATE scopeX_lib)

# ---- Install rules ----

if(NOT CMAKE_SKIP_INSTALL_RULES)
  include(cmake/install-rules.cmake)
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
