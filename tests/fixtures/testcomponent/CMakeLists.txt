# Fixture libraries for the component-loader tests.
#
# testcomponent    — a real component built on the SDK base classes.
# legacycomponent  — exports only the legacy CreateComponentInfo factory.
# notacomponent    — a valid shared library that is not a component at all.
# badabicomponent  — exports the entry points but reports a foreign ABI stamp.

add_library(testcomponent MODULE testcomponent.cpp)
target_link_libraries(testcomponent
    PRIVATE HydroCouple::HydroCouple HydroCoupleSDK::HydroCoupleSDK)
target_include_directories(testcomponent
    PRIVATE "${CMAKE_SOURCE_DIR}/include")
set_target_properties(testcomponent PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    SUFFIX "${COMPOSER_MODULE_SUFFIX}")

add_library(notacomponent MODULE notacomponent.cpp)
set_target_properties(notacomponent PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    SUFFIX "${COMPOSER_MODULE_SUFFIX}")

# Exports only the legacy Python-compatible factory, with no stamp.
add_library(legacycomponent MODULE legacycomponent.cpp)
target_link_libraries(legacycomponent
    PRIVATE HydroCouple::HydroCouple HydroCoupleSDK::HydroCoupleSDK)
set_target_properties(legacycomponent PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    SUFFIX "${COMPOSER_MODULE_SUFFIX}")

# Same entry points, deliberately wrong stamp — the mismatch path must be
# exercised by a library the loader can actually open.
add_library(badabicomponent MODULE badabicomponent.cpp)
target_link_libraries(badabicomponent PRIVATE HydroCouple::HydroCouple)
target_include_directories(badabicomponent
    PRIVATE "${CMAKE_SOURCE_DIR}/include")
set_target_properties(badabicomponent PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    SUFFIX "${COMPOSER_MODULE_SUFFIX}")
