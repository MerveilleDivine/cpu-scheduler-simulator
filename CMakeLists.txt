cmake_minimum_required(VERSION 3.13)

project(cpu_scheduler_simulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SCHEDULER_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(SCHEDULER_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

if(MSVC)
    add_compile_options(/W4)
    if(SCHEDULER_WARNINGS_AS_ERRORS)
        add_compile_options(/WX)
    endif()
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(SCHEDULER_WARNINGS_AS_ERRORS)
        add_compile_options(-Werror)
    endif()
endif()

if(SCHEDULER_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT MSVC)
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    else()
        message(FATAL_ERROR "Sanitizers require GCC or Clang.")
    endif()
endif()

add_library(scheduler_core src/Parser.cpp src/Scheduler.cpp src/Reporter.cpp)
target_include_directories(scheduler_core PUBLIC include)

add_executable(scheduler src/main.cpp)
target_link_libraries(scheduler PRIVATE scheduler_core)

include(CTest)
if(BUILD_TESTING)
    add_executable(scheduler_unit_tests tests/scheduler_tests.cpp)
    target_link_libraries(scheduler_unit_tests PRIVATE scheduler_core)
    add_test(NAME scheduler_unit_tests COMMAND scheduler_unit_tests)

    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME scheduler_cli_regression_tests
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/validate_json.py
                $<TARGET_FILE:scheduler> ${CMAKE_CURRENT_SOURCE_DIR}/examples/sample_input.txt)
        set_tests_properties(scheduler_cli_regression_tests PROPERTIES TIMEOUT 120)

        find_program(BASH_EXECUTABLE bash)
        if(UNIX AND BASH_EXECUTABLE)
            add_test(NAME scheduler_shell_smoke_tests
                COMMAND ${CMAKE_COMMAND} -E env SCHEDULER_BINARY=$<TARGET_FILE:scheduler>
                    ${BASH_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_tests.sh)
            set_tests_properties(scheduler_shell_smoke_tests PROPERTIES TIMEOUT 120)
        endif()
    else()
        message(STATUS "Python 3 not found: CLI regression tests are skipped; C++ unit tests remain enabled.")
    endif()
endif()
