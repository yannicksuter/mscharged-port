include_guard(GLOBAL)
include(cmake/Hierarchy.cmake)
include(cmake/SAnimAssets.cmake)
include(cmake/CompressedAssets.cmake)
add_library(charged_animation_bundle STATIC src/runtime/animation_bundle.cpp)
target_link_libraries(charged_animation_bundle PUBLIC charged_hierarchy_assets charged_sanim_assets
    charged_compressed_assets charged_decomp_startup)
if(BUILD_TESTING)
    add_executable(animation_bundle_tests tests/animation_bundle.cpp)
    target_link_libraries(animation_bundle_tests PRIVATE charged_animation_bundle aurora::dvd aurora::core)
    add_test(NAME animation_bundle
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_animation_bundle.py"
            "$<TARGET_FILE:animation_bundle_tests>")
    set_tests_properties(animation_bundle PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy" TIMEOUT 60)
endif()
