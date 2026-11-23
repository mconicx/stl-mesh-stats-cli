cmake_minimum_required(VERSION 3.20)
project(stl-mesh-stats-cli VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(STLSTATS_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(STLSTATS_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

add_library(stl_core INTERFACE)
target_include_directories(stl_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(MSVC)
  target_compile_options(stl_core INTERFACE /W4)
  if(STLSTATS_WARNINGS_AS_ERRORS)
    target_compile_options(stl_core INTERFACE /WX)
  endif()
else()
  target_compile_options(stl_core INTERFACE -Wall -Wextra -Wpedantic)
  if(STLSTATS_WARNINGS_AS_ERRORS)
    target_compile_options(stl_core INTERFACE -Werror)
  endif()
  if(STLSTATS_ENABLE_SANITIZERS)
    target_compile_options(stl_core INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer -g)
    target_link_options(stl_core INTERFACE -fsanitize=address,undefined)
  endif()
endif()

add_executable(stl-stats src/main.cpp)
target_link_libraries(stl-stats PRIVATE stl_core)

install(TARGETS stl-stats RUNTIME DESTINATION bin)

include(CTest)
if(BUILD_TESTING)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.18.0
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/stl_tests.cpp")
    add_executable(stl_tests tests/stl_tests.cpp)
    target_link_libraries(stl_tests PRIVATE stl_core GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(stl_tests)
  endif()
endif()