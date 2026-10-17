include_guard(GLOBAL)

# Keep ordinary engine regressions and application-loop fixtures consistent.
# This helper changes build bookkeeping only; it never registers or skips a test.
function(judas_add_engine_test target main_source)
    cmake_parse_arguments(TEST "APPLICATION" "" "SOURCES;LIBRARIES;WARNINGS" ${ARGN})
    if(TEST_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown arguments for ${target}: ${TEST_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${main_source}")
        message(FATAL_ERROR "Missing test entry point for ${target}: ${main_source}")
    endif()
    set(sources "${main_source}")
    if(TEST_APPLICATION)
        list(APPEND sources src/Application.cpp src/RuntimeOptions.cpp
            src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
    endif()
    add_executable(${target} ${sources} ${TEST_SOURCES})
    target_link_libraries(${target} PRIVATE judas_engine ${TEST_LIBRARIES})
    judas_warnings(${target} ${TEST_WARNINGS})
endfunction()
