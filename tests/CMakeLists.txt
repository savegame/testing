# Unit tests on SYNTHETIC data only — never real game files (CLAUDE.md §4).
add_executable(omw05-tests
    main.cpp
    test_core.cpp
    test_io_chunks.cpp
    test_io_compression.cpp
    test_formats_tpk.cpp
    test_formats_solids.cpp
    test_formats_track.cpp
)
target_link_libraries(omw05-tests PRIVATE omw05::core omw05::io omw05::formats omw05::doctest)
omw05_set_warnings(omw05-tests)
add_test(NAME omw05-tests COMMAND omw05-tests)
