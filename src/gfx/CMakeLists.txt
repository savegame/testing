# gfx module: GLES3 rendering (M0). Placeholder until GlContext/RenderTarget land.
add_library(omw05_gfx INTERFACE)
target_include_directories(omw05_gfx INTERFACE ${OMW05_INCLUDE_DIR})
target_link_libraries(omw05_gfx INTERFACE omw05::core)
add_library(omw05::gfx ALIAS omw05_gfx)
