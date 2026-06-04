# schemes.cmake - Parameterized build for scheme-specific fcitx5 addon
#
# Required CMake parameters:
#   -DSCHEME_ID=<id>           e.g., hanlo
#   -DSCHEME_NAME=<name>       e.g., 意傳教育部漢羅
#   -DSCHEME_SUBMODULE=<dir>   e.g., Rime-HanLo
#   -DSCHEME_ICON_TOO=<dir>    e.g., kip-hanlo (Rime-Logo subdirectory)
#
# Optional CMake parameters:
#   -DSCHEME_LABEL=<label>     e.g., 漢 (defaults to SCHEME_ID)
#   -DSCHEME_LANG_CODE=<code>  e.g., nan-TW (defaults to nan-TW)

# Validate required parameters
if(NOT SCHEME_ID)
    message(FATAL_ERROR "SCHEME_ID must be set (e.g. -DSCHEME_ID=hanlo)")
endif()
if(NOT SCHEME_NAME)
    message(FATAL_ERROR "SCHEME_NAME must be set (e.g. -DSCHEME_NAME=意傳教育部漢羅)")
endif()
if(NOT SCHEME_SUBMODULE)
    message(FATAL_ERROR "SCHEME_SUBMODULE must be set (e.g. -DSCHEME_SUBMODULE=Rime-HanLo)")
endif()
if(NOT SCHEME_ICON_TOO)
    message(FATAL_ERROR "SCHEME_ICON_TOO must be set (e.g. -DSCHEME_ICON_TOO=kip-hanlo)")
endif()

# Optional parameters with defaults
if(NOT SCHEME_LABEL)
    set(SCHEME_LABEL "${SCHEME_ID}")
endif()
if(NOT SCHEME_LANG_CODE)
    set(SCHEME_LANG_CODE "nan-TW")
endif()

message(STATUS "Building scheme: ${SCHEME_NAME} (${SCHEME_ID})")
message(STATUS "Scheme RIME_DATA_DIR: ${RIME_DATA_DIR}")

# Generate scheme_config.h from template
configure_file(
    "${CMAKE_SOURCE_DIR}/src/scheme_config.h.in"
    "${CMAKE_BINARY_DIR}/src/scheme_config.h"
    @ONLY
)

# Generate scheme_factory.cpp from template
configure_file(
    "${CMAKE_SOURCE_DIR}/src/scheme_factory.cpp.in"
    "${CMAKE_BINARY_DIR}/src/scheme_factory.cpp"
    @ONLY
)

# Source files (same as src/CMakeLists.txt but replacing rimefactory.cpp)
set(SCHEME_SOURCES
    "${CMAKE_SOURCE_DIR}/src/rimestate.cpp"
    "${CMAKE_SOURCE_DIR}/src/rimeengine.cpp"
    "${CMAKE_SOURCE_DIR}/src/rimecandidate.cpp"
    "${CMAKE_SOURCE_DIR}/src/rimesession.cpp"
    "${CMAKE_SOURCE_DIR}/src/rimeaction.cpp"
    "${CMAKE_BINARY_DIR}/src/scheme_factory.cpp"
)

set(SCHEME_LINK_LIBRARIES
    Fcitx5::Core
    Fcitx5::Config
    ${RIME_TARGET}
    Fcitx5::Module::Notifications
    Pthread::Pthread
)

# Optional DBus support
find_package(Fcitx5ModuleDBus QUIET)
if(Fcitx5ModuleDBus_FOUND)
    list(APPEND SCHEME_SOURCES "${CMAKE_SOURCE_DIR}/src/rimeservice.cpp")
    list(APPEND SCHEME_LINK_LIBRARIES Fcitx5::Module::DBus)
endif()

# Build the scheme addon
add_fcitx5_addon(${SCHEME_ID} ${SCHEME_SOURCES})
target_link_libraries(${SCHEME_ID} ${SCHEME_LINK_LIBRARIES})
target_include_directories(${SCHEME_ID} PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_BINARY_DIR}/src"
)
target_compile_definitions(${SCHEME_ID} PRIVATE HAVE_SCHEME_CONFIG)
if(NOT Fcitx5ModuleDBus_FOUND)
    target_compile_definitions(${SCHEME_ID} PRIVATE FCITX_RIME_NO_DBUS)
endif()

install(TARGETS ${SCHEME_ID} DESTINATION "${CMAKE_INSTALL_LIBDIR}/fcitx5")

# Generate and install InputMethod conf
configure_file(
    "${CMAKE_SOURCE_DIR}/src/scheme.conf.in"
    "${CMAKE_BINARY_DIR}/${SCHEME_ID}.conf"
    @ONLY
)
install(FILES "${CMAKE_BINARY_DIR}/${SCHEME_ID}.conf"
    DESTINATION "${FCITX_INSTALL_PKGDATADIR}/inputmethod"
    COMPONENT config
)

# Generate and install Addon conf (two-stage: @-substitution then i18n)
configure_file(
    "${CMAKE_SOURCE_DIR}/src/scheme-addon.conf.in.in"
    "${CMAKE_BINARY_DIR}/${SCHEME_ID}-addon.conf.in"
    @ONLY
)
fcitx5_translate_desktop_file(
    "${CMAKE_BINARY_DIR}/${SCHEME_ID}-addon.conf.in"
    ${SCHEME_ID}-addon.conf
)
install(FILES "${CMAKE_BINARY_DIR}/${SCHEME_ID}-addon.conf"
    RENAME ${SCHEME_ID}.conf
    DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon"
    COMPONENT config
)

# Install scheme data files from submodule
if(EXISTS "${CMAKE_SOURCE_DIR}/schemes/${SCHEME_SUBMODULE}/sujiphoat")
    file(GLOB SCHEME_DATA_FILES
        "${CMAKE_SOURCE_DIR}/schemes/${SCHEME_SUBMODULE}/sujiphoat/*.yaml")
    list(FILTER SCHEME_DATA_FILES EXCLUDE REGEX ".*\\.squirrel\\..*")
    install(FILES ${SCHEME_DATA_FILES} DESTINATION "${RIME_DATA_DIR}")
    message(STATUS "Scheme data files found: ${SCHEME_DATA_FILES}")
else()
    message(WARNING "Scheme data directory not found: schemes/${SCHEME_SUBMODULE}/sujiphoat/")
endif()

# Install icons
# Main icon from Rime-Logo submodule
# Status icons from existing fcitx5-rime icons, renamed per scheme
foreach(size 48x48 scalable)
    if("${size}" STREQUAL "scalable")
        set(ext "svg")
    else()
        set(ext "png")
    endif()

    # Main icon from Rime-Logo/${SCHEME_ICON_TOO}/fcitx-rime/
    set(main_icon "${CMAKE_SOURCE_DIR}/Rime-Logo/${SCHEME_ICON_TOO}/fcitx-rime/ithuan.${ext}")
    if(EXISTS "${main_icon}")
        install(FILES "${main_icon}"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}/apps"
            RENAME "fcitx-${SCHEME_ID}.${ext}")
    else()
        message(WARNING "Main icon not found: ${main_icon}")
    endif()

    # Status icons: copy from existing fcitx5-rime icons with new names
    foreach(status deploy disable im latin sync)
        set(src_icon "${CMAKE_SOURCE_DIR}/data/${size}/apps/fcitx_rime_${status}.${ext}")
        if(EXISTS "${src_icon}")
            install(FILES "${src_icon}"
                DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}/apps"
                RENAME "fcitx_${SCHEME_ID}_${status}.${ext}")
        endif()
    endforeach()
endforeach()
