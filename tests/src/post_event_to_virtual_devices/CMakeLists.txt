cmake_minimum_required (VERSION 3.9)

include (../../tests.cmake)

project (karabiner_test)

add_executable(
  karabiner_test
  src/test.cpp
)

add_executable(
  overwrite_expected_results
  src/overwrite_expected_results.cpp
)
