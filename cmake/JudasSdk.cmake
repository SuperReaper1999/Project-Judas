# Public native tools are declared once for staging, dependencies and verification.
# Include after all executable declarations so late subsystems cannot disappear.
find_package(Python3 3.11 COMPONENTS Interpreter QUIET)
if(NOT Python3_Interpreter_FOUND)
    message(STATUS "Judas SDK staging requires Python 3.11+; native runtime targets are still available")
    return()
endif()
option(JUDAS_SDK_RELEASE "Require clean known Release provenance when staging the SDK" OFF)
set(judas_sdk_preset "")
if(JUDAS_SDK_RELEASE)
    list(APPEND judas_sdk_preset --release)
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/packaging/sdk-inventory.json")
file(READ "${CMAKE_SOURCE_DIR}/packaging/sdk-inventory.json" judas_sdk_inventory)
set(JUDAS_SDK_TOOLS "")
set(judas_sdk_arrays tools)
if(WIN32)
    list(APPEND judas_sdk_arrays windows_tools)
endif()
foreach(array IN LISTS judas_sdk_arrays)
    string(JSON count LENGTH "${judas_sdk_inventory}" ${array})
    if(count EQUAL 0)
        continue()
    endif()
    math(EXPR last "${count} - 1")
    foreach(index RANGE 0 ${last})
        string(JSON tool GET "${judas_sdk_inventory}" ${array} ${index})
        if(NOT TARGET ${tool})
            message(FATAL_ERROR "Public SDK inventory names missing target: ${tool}")
        endif()
        list(APPEND JUDAS_SDK_TOOLS ${tool})
    endforeach()
endforeach()
if(WIN32)
    set(judas_sdk_target judas_windows_sdk)
    set(judas_sdk_directory windows-sdk)
else()
    set(judas_sdk_target judas_linux_sdk)
    set(judas_sdk_directory linux-sdk)
endif()
add_custom_target(${judas_sdk_target}
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/package_sdk.py" stage
        --source "${CMAKE_SOURCE_DIR}"
        --bin-dir "$<TARGET_FILE_DIR:judas>"
        --build-info "${CMAKE_BINARY_DIR}/judas-build-info-$<CONFIG>.json"
        --output "${CMAKE_BINARY_DIR}/${judas_sdk_directory}"
        --replace --probe-tools ${judas_sdk_preset}
    DEPENDS ${JUDAS_SDK_TOOLS}
    VERBATIM)
