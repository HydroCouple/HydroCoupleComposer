# docs/CMakeLists.txt — Doxygen documentation target.
#
# Mirrors HydroCoupleSDK's docs target so the three repositories render with
# one identical doxygen-awesome theme: same stylesheet, same header, same
# color style, same interactive SVG graphs.

set(DOXYGEN_IN  ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

# Doxygen supplied jQuery until version 1.17.
if(DOXYGEN_VERSION VERSION_LESS "1.17")
    set(DOXYGEN_JQUERY_SCRIPT [[<script type="text/javascript" src="$relpath^jquery.js"></script>]])
endif()
configure_file(custom/html/header.html header.html @ONLY)
configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

add_custom_target(docs
    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating Doxygen documentation for HydroCoupleComposer"
    VERBATIM
)

# Keep /guide/ and its chapter bookmarks stable across Doxygen's hashed output paths.
add_custom_command(TARGET docs POST_BUILD
    COMMAND "${CMAKE_COMMAND}" "-DSITE=${CMAKE_CURRENT_BINARY_DIR}/doxygen/html"
            "-DPREFIX=composer" -P "${CMAKE_CURRENT_SOURCE_DIR}/guide-links.cmake"
    VERBATIM
)
