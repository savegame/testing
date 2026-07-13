# io module: chunk container, compression, VFS (M1). Header-only until then.
add_library(omw05_io INTERFACE)
target_include_directories(omw05_io INTERFACE ${OMW05_INCLUDE_DIR})
target_link_libraries(omw05_io INTERFACE omw05::core)
add_library(omw05::io ALIAS omw05_io)
