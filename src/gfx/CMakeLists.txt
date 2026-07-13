# gfx module: GLES3 rendering. Parsers (formats/) never link this (CLAUDE.md §4).
find_package(SDL2 REQUIRED)

add_library(omw05_gfx STATIC
    GlContext.cpp
    Shader.cpp
    RenderTarget.cpp
    Compositor.cpp
    ImGuiLayer.cpp
)
target_include_directories(omw05_gfx PUBLIC ${OMW05_INCLUDE_DIR})
target_link_libraries(omw05_gfx PUBLIC omw05::core omw05::glad SDL2::SDL2)
if(OMW05_WITH_IMGUI)
    target_link_libraries(omw05_gfx PUBLIC omw05::imgui)
    target_compile_definitions(omw05_gfx PUBLIC OMW05_WITH_IMGUI)
endif()
omw05_set_warnings(omw05_gfx)
add_library(omw05::gfx ALIAS omw05_gfx)
