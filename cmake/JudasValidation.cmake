# Opt-in focused gates. Normal engine/game builds need no Python/npm tooling.
option(JUDAS_ENABLE_VALIDATION "Register bounded Pre-Alpha CTest gates" OFF)
option(JUDAS_INCLUDE_KNOWN_FAILURES "Also register explicitly open, failing contracts in CTest" OFF)
if(JUDAS_ENABLE_VALIDATION)
    find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
    # The CLI and CTest must accept the same declarations. In particular, a
    # malformed manual flag must not accidentally schedule a known failure.
    execute_process(COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/scripts/prealpha_validate.py" --check-manifest
        RESULT_VARIABLE judas_manifest_status OUTPUT_QUIET TIMEOUT 20
        ERROR_VARIABLE judas_manifest_error)
    if(NOT judas_manifest_status STREQUAL "0")
        message(FATAL_ERROR "Invalid focused validation manifest: ${judas_manifest_error}")
    endif()
    enable_testing()
    file(READ "${CMAKE_SOURCE_DIR}/tools/validation/suites.json" judas_suites)
    string(JSON judas_suite_count LENGTH "${judas_suites}" tests)
    math(EXPR judas_suite_last "${judas_suite_count} - 1")
    set(judas_gate_targets "")
    set(judas_ci_targets "")
    foreach(index RANGE ${judas_suite_last})
        string(JSON target GET "${judas_suites}" tests ${index} target)
        if(NOT TARGET ${target})
            message(FATAL_ERROR "Validation manifest names absent target: ${target}")
        endif()
        # Give each registered binary its own queryable identity. A neighboring
        # runtime or a timestamp cannot prove that an old test executable is current.
        get_target_property(has_validation_entry ${target} JUDAS_HAS_VALIDATION_ENTRY)
        if(NOT has_validation_entry)
            get_target_property(test_sources ${target} SOURCES)
            list(GET test_sources 0 test_main)
            file(READ "${CMAKE_SOURCE_DIR}/${test_main}" test_source)
            if(test_source MATCHES "int[ \t\r\n]+main[ \t\r\n]*[(][ \t\r\n]*(void)?[ \t\r\n]*[)]")
                target_compile_definitions(${target} PRIVATE JUDAS_VALIDATION_NO_ARGUMENTS)
            elseif(NOT test_source MATCHES "int[ \t\r\n]+main[ \t\r\n]*[(]")
                message(FATAL_ERROR "Focused validation target requires an explicit main: ${target}")
            endif()
            target_compile_definitions(${target} PRIVATE main=JudasValidationMain)
            target_sources(${target} PRIVATE tests/ValidationMain.cpp)
            target_link_libraries(${target} PRIVATE judas_build_identity)
            set_target_properties(${target} PROPERTIES JUDAS_HAS_VALIDATION_ENTRY TRUE)
        endif()
        string(JSON manual ERROR_VARIABLE manual_error GET "${judas_suites}" tests ${index} manual)
        if(NOT manual_error AND manual AND NOT JUDAS_INCLUDE_KNOWN_FAILURES)
            continue()
        endif()
        list(APPEND judas_gate_targets ${target})
        string(JSON name GET "${judas_suites}" tests ${index} name)
        string(JSON group_count LENGTH "${judas_suites}" tests ${index} groups)
        math(EXPR group_last "${group_count} - 1")
        set(labels prealpha)
        foreach(group_index RANGE ${group_last})
            string(JSON group GET "${judas_suites}" tests ${index} groups ${group_index})
            list(APPEND labels ${group})
        endforeach()
        add_test(NAME prealpha.${name}
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/prealpha_validate.py"
                --build-dir "$<TARGET_FILE_DIR:judas>" --test ${name}
                --expected-channel "${JUDAS_ID_channel}")
        string(JSON release_only ERROR_VARIABLE release_error GET "${judas_suites}" tests ${index} releaseOnly)
        if(release_error OR NOT release_only)
            list(APPEND labels routine)
            list(APPEND judas_ci_targets ${target})
        endif()
        string(JSON parallel ERROR_VARIABLE parallel_error GET "${judas_suites}" tests ${index} parallel)
        set(serial TRUE)
        if(NOT parallel_error AND parallel)
            set(serial FALSE)
        endif()
        set_tests_properties(prealpha.${name} PROPERTIES
            LABELS "${labels}" RUN_SERIAL ${serial} TIMEOUT 150)
    endforeach()
    list(REMOVE_DUPLICATES judas_gate_targets)
    add_custom_target(judas_prealpha_tests DEPENDS ${judas_gate_targets})
    list(REMOVE_DUPLICATES judas_ci_targets)
    add_custom_target(judas_ci_tests DEPENDS ${judas_ci_targets})
    add_test(NAME prealpha.editor-v2-architecture
        COMMAND ${Python3_EXECUTABLE} -m unittest discover -s "${CMAKE_SOURCE_DIR}/tests"
            -p test_editor_v2_architecture.py -v)
    set_tests_properties(prealpha.editor-v2-architecture PROPERTIES LABELS "prealpha;routine;editor;tooling")
    add_test(NAME prealpha.scripting-catalogue
        COMMAND ${Python3_EXECUTABLE} -m unittest discover -s "${CMAKE_SOURCE_DIR}/tests"
            -p test_scripting_catalogue.py -v)
    set_tests_properties(prealpha.scripting-catalogue PROPERTIES LABELS "prealpha;routine;tooling;scripting")
    add_test(NAME prealpha.gate-negative-controls
        COMMAND ${Python3_EXECUTABLE} -m unittest discover -s "${CMAKE_SOURCE_DIR}/tests"
            -p test_prealpha_validation.py -v)
    set_tests_properties(prealpha.gate-negative-controls PROPERTIES LABELS "prealpha;routine;tooling")
    add_test(NAME prealpha.browser-export-boundaries
        COMMAND ${Python3_EXECUTABLE} -m unittest discover -s "${CMAKE_SOURCE_DIR}/tests"
            -p "test_browser*.py" -v)
    set_tests_properties(prealpha.browser-export-boundaries PROPERTIES LABELS "prealpha;routine;tooling;browser")
    add_test(NAME prealpha.build-organization
        COMMAND ${Python3_EXECUTABLE} -m unittest discover -s "${CMAKE_SOURCE_DIR}/tests"
            -p test_cmake_test_targets.py -v)
    set_tests_properties(prealpha.build-organization PROPERTIES LABELS "prealpha;routine;tooling")
    add_test(NAME prealpha.gate-registration
        COMMAND ${Python3_EXECUTABLE} -m unittest discover -s "${CMAKE_SOURCE_DIR}/tests"
            -p test_validation_registration.py -v)
    set_tests_properties(prealpha.gate-registration PROPERTIES LABELS "prealpha;routine;tooling")
endif()
