# Builds Sound Mind Studio's MSIX package (v0.1.42.2) - run by the `msix`
# CMake target (see the top-level CMakeLists.txt), which passes:
#
#   BINARY_DIR      the configured build tree to install from
#   SOURCE_DIR      the repository root
#   OUTPUT_DIR      where the .msix goes
#   VERSION         the 4-part project version, X.Y.Z.W
#   ARCHITECTURE    x64 or arm64
#   IDENTITY_NAME, PUBLISHER, PUBLISHER_DISPLAY_NAME   the package identity
#   CRT_DLLS        the Visual C++ runtime DLLs to put in the package, "|"-separated
#   MAKEAPPX        path to makeappx.exe (Windows SDK)
#
# Steps: install the build into a fresh layout folder (the same files the zip
# and MSI ship, Qt deployment included), drop the VC++ redistributable
# installer (an MSIX can't run installers) and put the runtime DLLs in the
# package instead, add the tile images and the manifest, then makeappx pack.
# The result is unsigned: the Microsoft Store signs what it distributes, and
# a sideloaded copy needs signing with a trusted certificate first.

foreach (required BINARY_DIR SOURCE_DIR OUTPUT_DIR VERSION ARCHITECTURE IDENTITY_NAME PUBLISHER
                  PUBLISHER_DISPLAY_NAME MAKEAPPX)
    if (NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "make_msix.cmake: ${required} is not set")
    endif ()
endforeach ()

set(LAYOUT "${OUTPUT_DIR}/msix-layout-${ARCHITECTURE}")
file(REMOVE_RECURSE "${LAYOUT}")
file(MAKE_DIRECTORY "${LAYOUT}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${BINARY_DIR}" --prefix "${LAYOUT}"
    RESULT_VARIABLE install_result
)
if (NOT install_result EQUAL 0)
    message(FATAL_ERROR "make_msix.cmake: installing into the layout failed (${install_result})")
endif ()

file(GLOB redistributables "${LAYOUT}/vc_redist*.exe")
if (redistributables)
    file(REMOVE ${redistributables})
endif ()
string(REPLACE "|" ";" CRT_DLLS "${CRT_DLLS}")
if (NOT CRT_DLLS)
    message(FATAL_ERROR "make_msix.cmake: no Visual C++ runtime DLLs were found to package")
endif ()
foreach (dll IN LISTS CRT_DLLS)
    file(COPY "${dll}" DESTINATION "${LAYOUT}")
endforeach ()

file(COPY "${SOURCE_DIR}/packaging/msix/Assets" DESTINATION "${LAYOUT}")

# MSIX versions are Major.Minor.Build.Revision, each at most 65535, and the
# Store requires Revision 0 - so X.Y.Z.W becomes X.Y.(Z*1000+W).0, the same
# mapping the MSI's ProductVersion uses.
string(REPLACE "." ";" parts "${VERSION}")
list(GET parts 0 major)
list(GET parts 1 minor)
list(GET parts 2 feature)
list(GET parts 3 build)
math(EXPR packed "${feature} * 1000 + ${build}")
set(MSIX_VERSION "${major}.${minor}.${packed}.0")
set(MSIX_ARCHITECTURE "${ARCHITECTURE}")
set(MSIX_IDENTITY_NAME "${IDENTITY_NAME}")
set(MSIX_PUBLISHER "${PUBLISHER}")
set(MSIX_PUBLISHER_DISPLAY_NAME "${PUBLISHER_DISPLAY_NAME}")
configure_file("${SOURCE_DIR}/packaging/msix/AppxManifest.xml.in" "${LAYOUT}/AppxManifest.xml" @ONLY)

set(package "${OUTPUT_DIR}/sound-mind-studio-v${VERSION}-windows-${ARCHITECTURE}.msix")
execute_process(
    COMMAND "${MAKEAPPX}" pack /o /h SHA256 /d "${LAYOUT}" /p "${package}"
    RESULT_VARIABLE pack_result
    OUTPUT_VARIABLE pack_output
    ERROR_VARIABLE pack_output
)
if (NOT pack_result EQUAL 0)
    message(FATAL_ERROR "make_msix.cmake: makeappx failed (${pack_result}):\n${pack_output}")
endif ()
message(STATUS "Wrote ${package} (version ${MSIX_VERSION})")
