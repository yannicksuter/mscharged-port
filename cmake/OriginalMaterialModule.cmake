include_guard(GLOBAL)

# Whole original material compiler inventory. Compilation does not qualify
# original startup, shader execution, source resource backing or rendered output.
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8
        OR NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU" OR MSVC)
    return()
endif()
include(cmake/OriginalFunctionPools.cmake)
add_library(charged_original_material_program_sources OBJECT EXCLUDE_FROM_ALL
    "${MSCHARGED_PREPARED}/src/NL/glx/GXUnlitTextureMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXUnlitTextureMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXConstantColourMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXConstantColourMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXTextureColourAddMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXTextureColourAddMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXVertexColourTextureMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXVertexColourTextureMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXCompactColourMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXCompactColourMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXVertexColourMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXVertexColourMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXSkinnedUnlitTextureMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXSkinnedUnlitTextureMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXTextureBlendMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXTextureBlendMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXVertexColourDetailBlendMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXVertexColourDetailBlendMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXThreeLightDiffuseMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXThreeLightDiffuseMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXFixedLightMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXFixedLightMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXFloatTexturedColourMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXFloatTexturedColourMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXRedColourMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXRedColourMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXScissoredVertexColourTextureMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXScissoredVertexColourTextureMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXShadowVolumeMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXShadowVolumeMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXSkinnedMultiLightMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXSkinnedMultiLightMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXCharacterSkinCustomMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXCharacterSkinCustomMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXSpecularMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXSpecularMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXColourFresnelMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXColourFresnelMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXSpecularFresnelMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXSpecularFresnelMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXScrollingDiffuseMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXScrollingDiffuseMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXSpecularLookupMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXSpecularLookupMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXMovieMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXMovieMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXMaskedDetailBlendMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXMaskedDetailBlendMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXMaskedSpecularFresnelMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXMaskedSpecularFresnelMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXScrollingSpecularMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXScrollingSpecularMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXDetailModulateMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXDetailModulateMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXShadowedDetailBlendMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXShadowedDetailBlendMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXSpecularDetailBlendMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXSpecularDetailBlendMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXScrollingMaskedDetailBlendMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXScrollingMaskedDetailBlendMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXScrollingShadowedDetailBlendMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXScrollingShadowedDetailBlendMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXBlackTextureAlphaMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXBlackTextureAlphaMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXMegaDiffuseMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXMegaDiffuseMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXMegaSpecularFresnelMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXMegaSpecularFresnelMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXCharacterDamageMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXCharacterDamageMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXMegaSpecularMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXMegaSpecularMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXFourTextureAddMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXFourTextureAddMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXShadowedDiffuseMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXShadowedDiffuseMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXCameraScrolledOverlayMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXCameraScrolledOverlayMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXScrollingCameraOverlayMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXScrollingCameraOverlayMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXCrystalMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXCrystalMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXWarbleMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXWarbleMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXMaskedDiffuseBlendMaterialProgram.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXMaskedDiffuseBlendMaterialProgramRender.cpp"
    "${MSCHARGED_PREPARED}/src/NL/glx/GXMaterialProgramRegistry.cpp")
add_dependencies(charged_original_material_program_sources verify_prepared)
set_target_properties(charged_original_material_program_sources PROPERTIES
    POSITION_INDEPENDENT_CODE ON CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)
target_compile_features(charged_original_material_program_sources PRIVATE cxx_std_20)
target_include_directories(charged_original_material_program_sources BEFORE PRIVATE
    "${MSCHARGED_AURORA_PREPARED}/include")
target_include_directories(charged_original_material_program_sources PRIVATE
    "${MSCHARGED_PREPARED}/src"
    "${MSCHARGED_PREPARED}/src/NL/gl"
    "${MSCHARGED_PREPARED}/src/NL/glx")
target_link_libraries(charged_original_material_program_sources PRIVATE
    charged_original_function_pool_abi charged_native_metadata aurora::os)
target_compile_definitions(charged_original_material_program_sources PRIVATE
    MSCHARGED_GAME_MODULE=1 dSINGLE=1 __alloca=__builtin_alloca
    C_MTXFrustum=Charged_C_MTXFrustum C_MTXOrtho=Charged_C_MTXOrtho)
target_compile_options(charged_original_material_program_sources PRIVATE
    -ffp-contract=off -fno-strict-aliasing -fsigned-char -Wno-unknown-pragmas
    -fcheck-new -fno-rtti)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(charged_original_material_program_sources PRIVATE
        -fno-gnu-unique -fno-assume-sane-operators-new-delete)
else()
    target_compile_options(charged_original_material_program_sources PRIVATE
        -fno-assume-sane-operator-new -Wno-register)
endif()
add_custom_target(charged_original_material_program_scan
    COMMAND "${CMAKE_COMMAND}" "-DNM:FILEPATH=${CMAKE_NM}"
        "-DOBJECT:STRING=$<TARGET_OBJECTS:charged_original_material_program_sources>"
        "-DOUTPUT:FILEPATH=${CMAKE_CURRENT_BINARY_DIR}/original-material-program-undefined.txt"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ReportNativeSymbols.cmake"
    DEPENDS charged_original_material_program_sources
    COMMENT "Compile 87 whole original material TUs; complete link/resource/GPU admission pending"
    VERBATIM)
