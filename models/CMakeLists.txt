# CMakeLists.txt for model-specific sources
#
# Builds object libraries for model utilities so they can be folded into
# the main controller library. Add new model components here as they are
# introduced. Just add any new source files to the 'add_library(cpdn_models...)
# line. If the code gets too big we could consider making separate object
# 'libraries' per model e.g. cpdn_model_oifs, cpdn_model_hadam4. But for 
# now this approach keeps it simple.
#
# Object libraries keep the compiled .o files unarchived; we then merge
# them into the main controller library so we can reuse these helpers
# without creating extra static/shared libs per model.
#
#    Glenn Carver, CPDN, 2025.

# Object library for OpenIFS model helpers
add_library(cpdn_models OBJECT
    ${CMAKE_CURRENT_SOURCE_DIR}/openifs/oifs_utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/test_control.cpp
)


# Expose project-root includes so callers can use headers such as
# "models/openifs/oifs_utils.h".
target_include_directories(cpdn_models PUBLIC
    ${PROJECT_SOURCE_DIR}
)
