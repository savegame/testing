# Vendored third-party dependencies ONLY (see CLAUDE.md §4 and THIRD_PARTY.md).
# SDL2 comes from the system via find_package, not from here.

# glad loader generated for the GLES2 API at version 3.0 — the only GL API
# surface this project may use (CLAUDE.md §2).
add_library(omw05_glad STATIC glad/src/gles2.c)
target_include_directories(omw05_glad SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/glad/include)
add_library(omw05::glad ALIAS omw05_glad)

if(OMW05_WITH_IMGUI AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui.h)
    add_library(omw05_imgui STATIC
        imgui/imgui.cpp
        imgui/imgui_demo.cpp
        imgui/imgui_draw.cpp
        imgui/imgui_tables.cpp
        imgui/imgui_widgets.cpp
        imgui/backends/imgui_impl_sdl2.cpp
        imgui/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(omw05_imgui SYSTEM PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends
    )
    # The GLES3 renderer backend path (#version 300 es) — see CLAUDE.md §5b.
    target_compile_definitions(omw05_imgui PUBLIC IMGUI_IMPL_OPENGL_ES3)
    find_package(SDL2 REQUIRED)
    target_link_libraries(omw05_imgui PUBLIC SDL2::SDL2)
    # With IMGUI_IMPL_OPENGL_ES3 the backend calls GLES entry points directly
    # (no embedded loader), so it needs the system GLES 3.0 library (ANGLE's
    # libGLESv2 on Windows).
    find_library(OMW05_GLES2_LIBRARY NAMES GLESv2 libGLESv2 REQUIRED)
    target_link_libraries(omw05_imgui PUBLIC ${OMW05_GLES2_LIBRARY})
    add_library(omw05::imgui ALIAS omw05_imgui)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/doctest/doctest.h)
    add_library(omw05_doctest INTERFACE)
    target_include_directories(omw05_doctest SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/doctest)
    add_library(omw05::doctest ALIAS omw05_doctest)
endif()
