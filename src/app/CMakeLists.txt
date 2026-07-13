# omw05-viewer: the interactive viewer executable (M0).
add_executable(omw05-viewer
    main.cpp
    Viewer.cpp
)
target_link_libraries(omw05-viewer PRIVATE omw05::core omw05::gfx)
omw05_set_warnings(omw05-viewer)
