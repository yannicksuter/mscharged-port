include_guard(GLOBAL)

# Host-only loader/file incarnation service. Adding this library does not admit
# a Windows original-game profile or relax its unresolved-import/LLP64 gates.
get_filename_component(_module_loader_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
add_library(charged_native_module_loader STATIC "${_module_loader_root}/src/platform/native_module_loader.cpp")
target_include_directories(charged_native_module_loader PUBLIC "${_module_loader_root}/src")
target_compile_features(charged_native_module_loader PUBLIC cxx_std_17)
target_link_libraries(charged_native_module_loader PRIVATE ${CMAKE_DL_LIBS})

if(BUILD_TESTING)
    add_executable(native_module_loader_tests "${_module_loader_root}/tests/native_module_loader.cpp")
    target_link_libraries(native_module_loader_tests PRIVATE charged_native_module_loader)
    set_target_properties(native_module_loader_tests PROPERTIES ENABLE_EXPORTS ON)
    add_library(native_module_loader_fixture SHARED "${_module_loader_root}/tests/fixtures/native_module_loader.cpp")
    add_library(native_module_loader_foreign SHARED "${_module_loader_root}/tests/fixtures/native_module_loader.cpp")
    target_compile_definitions(native_module_loader_foreign PRIVATE FIXTURE_FOREIGN=1)
    foreach(_fixture native_module_loader_fixture native_module_loader_foreign)
        target_link_libraries(${_fixture} PRIVATE native_module_loader_tests)
        target_compile_features(${_fixture} PRIVATE cxx_std_17)
    endforeach()
    add_test(NAME native_module_loader COMMAND native_module_loader_tests
        "$<TARGET_FILE:native_module_loader_fixture>" "$<TARGET_FILE:native_module_loader_foreign>"
        "${CMAKE_CURRENT_BINARY_DIR}/module-loader-test-data")
    if(WIN32 AND CMAKE_DLLTOOL)
        # A real PE imports a deliberately unavailable executable export. The
        # import library declares the contract; it supplies no implementation.
        set(_missing_def "${CMAKE_CURRENT_BINARY_DIR}/native_module_loader_missing.def")
        file(GENERATE OUTPUT "${_missing_def}" CONTENT
            "LIBRARY $<TARGET_FILE_NAME:native_module_loader_tests>\nEXPORTS\nfixture_host_event\nfixture_absent_host_service\n")
        set(_missing_lib "${CMAKE_CURRENT_BINARY_DIR}/native_module_loader_missing.a")
        add_custom_command(OUTPUT "${_missing_lib}"
            COMMAND "${CMAKE_DLLTOOL}" -m i386:x86-64 -d "${_missing_def}" -l "${_missing_lib}"
            DEPENDS "${_missing_def}" VERBATIM)
        add_library(native_module_loader_missing SHARED "${_module_loader_root}/tests/fixtures/native_module_loader.cpp" "${_missing_lib}")
        target_compile_definitions(native_module_loader_missing PRIVATE FIXTURE_MISSING_IMPORT=1)
        target_link_libraries(native_module_loader_missing PRIVATE "${_missing_lib}")
        add_test(NAME native_module_loader_missing_imports COMMAND native_module_loader_tests
            "$<TARGET_FILE:native_module_loader_fixture>" "$<TARGET_FILE:native_module_loader_foreign>"
            "${CMAKE_CURRENT_BINARY_DIR}/module-loader-missing-test-data" "$<TARGET_FILE:native_module_loader_missing>")
        set_tests_properties(native_module_loader_missing_imports PROPERTIES TIMEOUT 20)
    endif()
    if(MINGW)
        foreach(_fixture native_module_loader_tests native_module_loader_fixture
                native_module_loader_foreign native_module_loader_missing)
            if(TARGET "${_fixture}")
                target_link_options("${_fixture}" PRIVATE -static)
            endif()
        endforeach()
    endif()
    set_tests_properties(native_module_loader PROPERTIES TIMEOUT 20)
endif()
