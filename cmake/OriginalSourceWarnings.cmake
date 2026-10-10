include_guard(GLOBAL)

# GCC reports, by default, several console idioms that the reconstructed game
# sources and their headers use on purpose: four-character constants ('RLYT'),
# classes in a header's anonymous namespace (nw4hbm snd) and offsetof on
# non-standard-layout game classes. Every project target that includes the
# original headers sees them. Call this once after all targets are defined: it
# adds the GCC-only suppressions to targets of this directory only, so the
# third-party subdirectories (SDL, Aurora, Dawn, nod) keep their flags and are
# not rebuilt. Clang keeps its own policy.
function(mscharged_apply_gnu_original_warning_policy)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        return()
    endif()
    get_property(targets DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target IN LISTS targets)
        get_target_property(type ${target} TYPE)
        if(type MATCHES "^(STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|OBJECT_LIBRARY|EXECUTABLE)$")
            target_compile_options(${target} PRIVATE
                "$<$<COMPILE_LANGUAGE:C,CXX>:-Wno-multichar>"
                "$<$<COMPILE_LANGUAGE:CXX>:-Wno-subobject-linkage;-Wno-invalid-offsetof>")
        endif()
    endforeach()
    # False positive: the path length is checked against FILE_PATH_MAX before
    # the strncat whose bound GCC questions.
    set_property(SOURCE "${MSCHARGED_PREPARED}/src/RVL_SDK/hbm/nw4hbm/snd/snd_SoundArchive.cpp"
        APPEND PROPERTY COMPILE_OPTIONS -Wno-stringop-overflow)
endfunction()

# AppleClang enables additional diagnostics for the unchanged retail C++.
# Apply these only to prepared game sources: native adapters, tests and third-
# party targets keep their diagnostics. Linux (including Clang) and Windows
# retain their existing compiler options.
function(mscharged_apply_macos_original_warning_policy)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR
            NOT CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
        return()
    endif()

    get_property(targets DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY BUILDSYSTEM_TARGETS)
    set(original_sources)
    foreach(target IN LISTS targets)
        get_target_property(sources ${target} SOURCES)
        foreach(source IN LISTS sources)
            string(FIND "${source}" "${MSCHARGED_PREPARED}/" prefix)
            if(prefix EQUAL 0)
                list(APPEND original_sources "${source}")
            endif()
        endforeach()
    endforeach()
    list(REMOVE_DUPLICATES original_sources)
    if(original_sources)
        # Partial enum switches, char* literal tables and C-linkage names for
        # C++ functions are source conventions. Like the GNU original-module
        # policy, allow integers stored in pointer-typed contexts; pointer-to-
        # integer truncation remains diagnosed.
        set_property(SOURCE ${original_sources} APPEND PROPERTY COMPILE_OPTIONS
            "$<$<COMPILE_LANGUAGE:CXX>:-Wno-switch;-Wno-writable-strings;-Wno-return-type-c-linkage;-Wno-int-to-pointer-cast;-Wno-int-to-void-pointer-cast>")
    endif()

    # Keep the original argument order/conversion in this retail call.
    set_property(SOURCE "${MSCHARGED_PREPARED}/src/Game/AI/SpaceSearch.cpp"
        APPEND PROPERTY COMPILE_OPTIONS
        "$<$<COMPILE_LANGUAGE:CXX>:-Wno-implicit-conversion-floating-point-to-bool>")

    # Both pinned sources explicitly document the retail uninitialized height
    # read before clamping. Preserve it, with no blanket uninitialized-warning
    # suppression for the rest of the game or the native layer.
    set_property(SOURCE
        "${MSCHARGED_PREPARED}/src/Game/Render/BirdoEgg.cpp"
        "${MSCHARGED_PREPARED}/src/Game/Render/KoopaShellObject.cpp"
        APPEND PROPERTY COMPILE_OPTIONS "$<$<COMPILE_LANGUAGE:CXX>:-Wno-uninitialized>")
endfunction()
