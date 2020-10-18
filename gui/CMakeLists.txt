set(REQUIRED_QT5_VERSION 5.7)

find_package(Qt5 ${REQUIRED_QT5_VERSION} CONFIG REQUIRED COMPONENTS Core Widgets)
find_package(Fcitx5Qt5WidgetsAddons REQUIRED)

set(RIME_CONFIG_SRCS
  Main.cpp
  ConfigMain.cpp
  Model.cpp
  RimeConfigParser.cpp
  keylistwidget.cpp)

set(RIME_CONFIG_HDRS
  Main.h
  ConfigMain.h
  Model.h
  Common.h
  RimeConfigParser.h
  keylistwidget.h)

add_library(rime-config
  MODULE ${RIME_CONFIG_SRCS})

target_include_directories(rime-config PRIVATE ${PROJECT_SOURCE_DIR}/gui)

set_target_properties(rime-config PROPERTIES
  LINK_FLAGS "-Wl,--no-undefined"
  AUTOMOC TRUE
  AUTOUIC TRUE
  AUTOUIC_OPTIONS "-tr=fcitx::tr2fcitx;--include=fcitxqti18nhelper.h")

target_link_libraries(rime-config
  Qt5::Widgets
  Fcitx5::Core
  Fcitx5Qt5::WidgetsAddons
  PkgConfig::Rime)

install(TARGETS rime-config DESTINATION ${CMAKE_INSTALL_LIBDIR}/fcitx5/qt5)
