# docs/CMakeLists.txt — Doxygen documentation target.
#
# Mirrors HydroCoupleSDK's docs target so the three repositories render with
# one identical doxygen-awesome theme: same stylesheet, same header, same
# colour style, same interactive SVG graphs.

set(DOXYGEN_IN  ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

add_custom_target(docs
    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating Doxygen documentation for HydroCoupleComposer"
    VERBATIM
)
