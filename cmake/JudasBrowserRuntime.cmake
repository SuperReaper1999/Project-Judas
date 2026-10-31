target_sources(judas_engine PRIVATE src/BrowserPlatform.cpp)
target_include_directories(judas_engine SYSTEM PRIVATE third_party/cgltf)
target_link_libraries(judas_engine PUBLIC judas_quickjs judas_unicode judas_profiler judas_jobs)
target_compile_options(judas_engine PUBLIC -fexceptions)
add_executable(judas_browser src/BrowserEntry.cpp src/main.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_browser PRIVATE judas_engine)
set_target_properties(judas_browser PROPERTIES SUFFIX ".html")
set_property(TARGET judas_browser APPEND PROPERTY LINK_DEPENDS "${CMAKE_SOURCE_DIR}/platform/browser/shell.html")
target_link_options(judas_browser PRIVATE -fexceptions -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2
 -sASYNCIFY=1 -sASYNCIFY_STACK_SIZE=65536 -sPTHREAD_POOL_SIZE=8 -sOFFSCREENCANVAS_SUPPORT=1
 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=268435456 -sMAXIMUM_MEMORY=1073741824
 -sSTACK_SIZE=8388608 -sEXIT_RUNTIME=1 -sFORCE_FILESYSTEM=1 -lidbfs.js
 "-sEXPORTED_RUNTIME_METHODS=['FS','IDBFS','addRunDependency','removeRunDependency','callMain','ENV','FS_createPath','FS_createDataFile']"
 "--shell-file" "${CMAKE_SOURCE_DIR}/platform/browser/shell.html")
add_executable(judas_browser_physics_tests tests/Mk2PhysicsTests.cpp ${JUDAS_PHYSICS_SOURCES} ${JUDAS_PHYSICS_WORLD_SOURCES})
target_include_directories(judas_browser_physics_tests PRIVATE src)
target_link_libraries(judas_browser_physics_tests PRIVATE ${JUDAS_GLM} judas_profiler judas_jobs)
target_link_options(judas_browser_physics_tests PRIVATE -sPTHREAD_POOL_SIZE=2 -sEXIT_RUNTIME=1 -sSTACK_SIZE=8388608 -sINITIAL_MEMORY=67108864)
