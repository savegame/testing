# io module: chunk container, chunk IDs, compression, VFS (M1).
add_library(omw05_io STATIC
    ChunkIds.cpp
    ChunkReader.cpp
)
target_include_directories(omw05_io PUBLIC ${OMW05_INCLUDE_DIR})
target_link_libraries(omw05_io PUBLIC omw05::core)
omw05_set_warnings(omw05_io)
add_library(omw05::io ALIAS omw05_io)
