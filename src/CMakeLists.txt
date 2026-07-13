# Module libraries. Layering (CLAUDE.md §4): core <- io <- formats (CPU only,
# never touch GL) and core <- gfx <- app. Tools/tests link the CPU-side libs
# and must build headless.

set(OMW05_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)

add_subdirectory(core)
add_subdirectory(io)
add_subdirectory(formats)
add_subdirectory(gfx)
add_subdirectory(app)
