# omw05-viewer: the interactive viewer executable (M0).
add_executable(omw05-viewer
    main.cpp
    Viewer.cpp
)
target_link_libraries(omw05-viewer PRIVATE omw05::core omw05::io omw05::formats omw05::gfx)
target_include_directories(omw05-viewer SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/extern/stb)
omw05_set_warnings(omw05-viewer)
