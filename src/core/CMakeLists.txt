add_library(omw05_core STATIC
    Log.cpp
)
target_include_directories(omw05_core PUBLIC ${OMW05_INCLUDE_DIR})
omw05_set_warnings(omw05_core)
add_library(omw05::core ALIAS omw05_core)
