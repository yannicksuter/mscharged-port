include_guard(GLOBAL)

# Both standalone SaveLoad palettes and embedded HOME ARC files use this one
# binder/registry. Adding exact ARC bounds does not select any game resource.
function(mscharged_select_native_tpl_transport target)
    get_target_property(_sources "${target}" SOURCES)
    foreach(_path IN ITEMS
            "${PROJECT_SOURCE_DIR}/src/platform/native_tpl.cpp"
            "${PROJECT_SOURCE_DIR}/src/platform/arc_data_transport.cpp")
        file(RELATIVE_PATH _relative "${PROJECT_SOURCE_DIR}" "${_path}")
        if(NOT _path IN_LIST _sources AND NOT _relative IN_LIST _sources)
            target_sources("${target}" PRIVATE "${_path}")
            list(APPEND _sources "${_path}")
        endif()
    endforeach()
    target_compile_features("${target}" PRIVATE cxx_std_20)
    target_include_directories("${target}" PRIVATE "${PROJECT_SOURCE_DIR}/src")
endfunction()

# The whole original TPLGet TU belongs beside its caller's sole ownership
# registry. Native TPLBind still supplies Wii pointer/endian software relocation;
# the original modulo/index method and all console source remain unchanged.
function(mscharged_add_original_tpl target)
    get_target_property(_type "${target}" TYPE)
    if(NOT _type STREQUAL "MODULE_LIBRARY")
        message(FATAL_ERROR "Original TPL must share its source module's allocation/completion registry")
    endif()
    include(cmake/OriginalARC.cmake)
    mscharged_add_original_arc("${target}")
    mscharged_select_native_tpl_transport("${target}")
    get_target_property(_sources "${target}" SOURCES)
    set(_tpl "${MSCHARGED_PREPARED}/src/RVL_SDK/tpl/TPL.c")
    if(NOT _tpl IN_LIST _sources)
        target_sources("${target}" PRIVATE "${_tpl}")
    endif()
    # Public fixed32 address cells are native C++ views.
    set_property(SOURCE "${_tpl}" TARGET_DIRECTORY "${target}" PROPERTY LANGUAGE CXX)
endfunction()

if(BUILD_TESTING AND TARGET aurora::gx)
    include(cmake/NativeHBMDebug.cmake)
    add_executable(native_hbm_tpl_tests tests/native_hbm_tpl.cpp
        "${MSCHARGED_PREPARED}/src/NL/MemAlloc.cpp"
        "${MSCHARGED_PREPARED}/src/RVL_SDK/tpl/TPL.c"
        "${MSCHARGED_PREPARED}/src/RVL_SDK/hbm/nw4hbm/lyt/lyt_common.cpp"
        src/platform/game_allocation_ownership.cpp src/platform/host_metadata.cpp)
    mscharged_select_native_tpl_transport(native_hbm_tpl_tests)
    set_property(SOURCE "${MSCHARGED_PREPARED}/src/RVL_SDK/tpl/TPL.c"
        TARGET_DIRECTORY native_hbm_tpl_tests PROPERTY LANGUAGE CXX)
    add_dependencies(native_hbm_tpl_tests verify_prepared)
    target_include_directories(native_hbm_tpl_tests PRIVATE
        "${MSCHARGED_PREPARED}/include" "${MSCHARGED_PREPARED}/libs/RVL_SDK/include"
        # Test observation uses the actual SDK's descriptor definition. No
        # mirrored layout, renderer initialization or GPU success substitute.
        "${MSCHARGED_AURORA_PREPARED}/lib")
    target_compile_definitions(native_hbm_tpl_tests PRIVATE
        MSCHARGED_NATIVE=1 MSCHARGED_GAME_MODULE=1 TARGET_PC=1 HBM_ASSERT=1 WEBGPU_DAWN)
    target_compile_options(native_hbm_tpl_tests PRIVATE -fshort-wchar -fno-rtti
        -fno-strict-aliasing -ffp-contract=off -ffunction-sections -fdata-sections
        -Wno-unknown-pragmas "$<$<CXX_COMPILER_ID:Clang>:-Wno-register>")
    target_link_libraries(native_hbm_tpl_tests PRIVATE aurora::os aurora::gx Threads::Threads)
    mscharged_link_original_hbm_debug_host(native_hbm_tpl_tests CPU_FIXTURE)
    if(APPLE)
        target_link_options(native_hbm_tpl_tests PRIVATE "LINKER:-dead_strip")
    else()
        target_link_options(native_hbm_tpl_tests PRIVATE "LINKER:--gc-sections")
    endif()
    add_test(NAME native_hbm_tpl COMMAND native_hbm_tpl_tests)
    set_tests_properties(native_hbm_tpl PROPERTIES TIMEOUT 15 LABELS "Platform")
endif()
