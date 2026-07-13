# mw-* CLI utilities, built from the same module libraries (CLAUDE.md §4).
# All tools are headless: they must never link omw05::gfx.

add_executable(mw-chunkdump mw-chunkdump.cpp)
target_link_libraries(mw-chunkdump PRIVATE omw05::core omw05::io)
omw05_set_warnings(mw-chunkdump)

add_executable(mw-texdump mw-texdump.cpp)
target_link_libraries(mw-texdump PRIVATE omw05::core omw05::io omw05::formats)
target_include_directories(mw-texdump SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/extern/stb)
omw05_set_warnings(mw-texdump)
