cmake_minimum_required(VERSION 3.6.0)

project(fcitx5-rime VERSION 5.1.9)

# Ubuntu 24.04 LTS ships fcitx5 5.1.7. This compat branch targets that floor so
# the .deb installs against the distro's stock fcitx5 (no PPA required).
set(REQUIRED_FCITX_VERSION 5.1.7)

find_package(ECM 1.0.0 REQUIRED)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
include(FeatureSummary)
include(GNUInstallDirs)
include(ECMSetupVersion)
include(ECMUninstallTarget)

find_package(Gettext REQUIRED)
find_package(Fcitx5Core ${REQUIRED_FCITX_VERSION} REQUIRED)
find_package(Fcitx5Module REQUIRED COMPONENTS Notifications)
find_package(PkgConfig REQUIRED)
find_package(Pthread REQUIRED)

if (NOT DEFINED RIME_TARGET)
    pkg_check_modules(Rime REQUIRED IMPORTED_TARGET "rime>=1.0.0")
    set(RIME_TARGET PkgConfig::Rime)
endif()

if ("${Rime_VERSION}" VERSION_LESS "1.10.0")
    add_definitions(-DFCITX_RIME_NO_HIGHLIGHT_CANDIDATE)
endif()
if ("${Rime_VERSION}" VERSION_LESS "1.8.0")
    add_definitions(-DFCITX_RIME_NO_DELETE_CANDIDATE)
endif()
if ("${Rime_VERSION}" VERSION_LESS "1.7.0")
    add_definitions(-DFCITX_RIME_LOAD_PLUGIN)
endif()
if ("${Rime_VERSION}" VERSION_LESS "1.6.0")
    add_definitions(-DFCITX_RIME_NO_LOG_LEVEL)
endif()
if ("${Rime_VERSION}" VERSION_LESS "1.3.0")
    add_definitions(-DFCITX_RIME_NO_SELECT_CANDIDATE)
endif()

if(SCHEME_ID)
  set(RIME_DATA_DIR "${CMAKE_INSTALL_PREFIX}/share/${SCHEME_ID}/data")
elseif(NOT DEFINED RIME_DATA_DIR)
  find_package(RimeData REQUIRED)
endif()
message(STATUS "Precompiler macro RIME_DATA_DIR is set to \"${RIME_DATA_DIR}\"")
add_definitions(-DRIME_DATA_DIR="${RIME_DATA_DIR}")
if(SCHEME_ID)
  add_definitions(-DFCITX_GETTEXT_DOMAIN=\"fcitx5-${SCHEME_ID}\")
else()
  add_definitions(-DFCITX_GETTEXT_DOMAIN=\"fcitx5-rime\")
endif()
add_definitions(-DFCITX_RIME_VERSION=\"${PROJECT_VERSION}\")
fcitx5_add_i18n_definition()

include("${FCITX_INSTALL_CMAKECONFIG_DIR}/Fcitx5Utils/Fcitx5CompilerSettings.cmake")

add_subdirectory(po)

if(SCHEME_ID)
  include(schemes.cmake)
else()
  add_subdirectory(src)
  add_subdirectory(data)

  fcitx5_translate_desktop_file(org.fcitx.Fcitx5.Addon.Rime.metainfo.xml.in
                                org.fcitx.Fcitx5.Addon.Rime.metainfo.xml XML)

  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/org.fcitx.Fcitx5.Addon.Rime.metainfo.xml" DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

