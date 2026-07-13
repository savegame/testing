# formats module: CPU-side parsers only, must never touch GL (CLAUDE.md §4).
add_library(omw05_formats STATIC
    TexturePack.cpp
    Solids.cpp
    TrackStreamer.cpp
)
target_include_directories(omw05_formats PUBLIC ${OMW05_INCLUDE_DIR})
target_link_libraries(omw05_formats PUBLIC omw05::io)
omw05_set_warnings(omw05_formats)
add_library(omw05::formats ALIAS omw05_formats)
