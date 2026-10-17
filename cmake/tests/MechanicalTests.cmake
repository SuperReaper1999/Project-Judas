include("${CMAKE_CURRENT_LIST_DIR}/../JudasTestTargets.cmake")

# M79–M83 mechanisms retain their original entry points, sources and warnings.
# Gameplay and authoring policies remain in their ordinary project fixtures.
judas_add_engine_test(judas_structural_tests tests/StructuralTests.cpp WARNINGS -Wpedantic)
judas_add_engine_test(judas_structural_application_tests tests/StructuralApplicationTests.cpp
    APPLICATION WARNINGS -Wpedantic)
judas_add_engine_test(judas_bending_tests tests/BendingTests.cpp WARNINGS -Wpedantic)
judas_add_engine_test(judas_bending_application_tests tests/BendingApplicationTests.cpp
    APPLICATION WARNINGS -Wpedantic)

judas_add_engine_test(judas_wheel_tests tests/WheelTests.cpp WARNINGS -Wpedantic)
judas_add_engine_test(judas_wheel_script_tests tests/WheelScriptTests.cpp WARNINGS -Wpedantic)
judas_add_engine_test(judas_wheel_lifecycle_tests tests/WheelLifecycleTests.cpp WARNINGS -Wpedantic)
judas_add_engine_test(judas_wheel_application_tests tests/WheelApplicationTests.cpp
    APPLICATION WARNINGS -Wpedantic)

judas_add_engine_test(judas_powertrain_tests tests/PowertrainTests.cpp WARNINGS -Wpedantic)
judas_add_engine_test(judas_powertrain_script_tests tests/PowertrainScriptTests.cpp WARNINGS -Wpedantic)
judas_add_engine_test(judas_powertrain_lifecycle_tests tests/PowertrainLifecycleTests.cpp WARNINGS -Wpedantic)
judas_add_engine_test(judas_powertrain_application_tests tests/PowertrainApplicationTests.cpp
    APPLICATION WARNINGS -Wpedantic)

judas_add_engine_test(judas_mechanical_damage_tests tests/MechanicalDamageTests.cpp WARNINGS -Wpedantic)
judas_add_engine_test(judas_mechanical_connection_tests tests/MechanicalConnectionTests.cpp WARNINGS -Wpedantic)
judas_add_engine_test(judas_mechanical_damage_application_tests tests/MechanicalDamageApplicationTests.cpp
    APPLICATION WARNINGS -Wpedantic)
judas_add_engine_test(judas_mechanical_crash_application_tests tests/MechanicalCrashApplicationTests.cpp
    APPLICATION WARNINGS -Wpedantic)
judas_add_engine_test(judas_vehicle_damage_application_tests tests/MechanicalVehicleCrashApplicationTests.cpp
    APPLICATION WARNINGS -Wpedantic)
judas_add_engine_test(judas_mechanical_damage_script_tests tests/MechanicalDamageScriptTests.cpp WARNINGS -Wpedantic)
judas_add_engine_test(judas_section_material_tests tests/SectionMaterialTests.cpp WARNINGS -Wpedantic)
judas_add_engine_test(judas_coarse_crash_application_tests tests/CoarseCrashApplicationTests.cpp
    APPLICATION WARNINGS -Wpedantic)
