include_guard(GLOBAL)

if(BUILD_TESTING)
    find_package(Threads REQUIRED)
    # A direct original allocator/data-domain gate. The test's two C metadata
    # functions inject host allocation failures; game allocation is real MemAlloc.
    add_executable(original_byte_domain_tests
        tests/original_byte_domains.cpp
        "${MSCHARGED_PREPARED}/src/NL/MemAlloc.cpp"
        src/platform/game_allocation_ownership.cpp)
    add_dependencies(original_byte_domain_tests verify_prepared)
    target_compile_features(original_byte_domain_tests PRIVATE cxx_std_17)
    target_include_directories(original_byte_domain_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${MSCHARGED_PREPARED}/include"
        "${MSCHARGED_PREPARED}/libs/RVL_SDK/include")
    target_compile_definitions(original_byte_domain_tests PRIVATE
        MSCHARGED_NATIVE=1 MSCHARGED_GAME_MODULE=1 TARGET_PC=1)
    target_link_libraries(original_byte_domain_tests PRIVATE Threads::Threads)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU" AND NOT MSVC)
        target_compile_options(original_byte_domain_tests PRIVATE
            -fno-strict-aliasing -ffp-contract=off -fsigned-char -Wno-unknown-pragmas)
    endif()
    add_test(NAME original_byte_domains COMMAND original_byte_domain_tests)
    set_tests_properties(original_byte_domains PROPERTIES TIMEOUT 20)
endif()
