add_executable(scopeX_tests EXCLUDE_FROM_ALL
  source/engine/test_engine_basic.cpp
)

target_link_libraries(scopeX_tests
  PRIVATE
    scopeX::engine          
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(scopeX_tests
  PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(scopeX_tests PRIVATE cxx_std_20)
target_compile_options(scopeX_tests PRIVATE -Wall -Wextra -Wpedantic)

include(GoogleTest)
gtest_discover_tests(scopeX_tests
  DISCOVERY_MODE PRE_TEST     
  DISCOVERY_TIMEOUT 60
)

add_custom_target(check
  DEPENDS scopeX_tests
  COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)