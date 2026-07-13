# formats module: CPU-side parsers only, must never touch GL (CLAUDE.md §4).
add_library(omw05_formats INTERFACE)
target_include_directories(omw05_formats INTERFACE ${OMW05_INCLUDE_DIR})
target_link_libraries(omw05_formats INTERFACE omw05::io)
add_library(omw05::formats ALIAS omw05_formats)
