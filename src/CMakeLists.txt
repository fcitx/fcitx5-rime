set(RIME_SOURCES
    rimestate.cpp
    rimeengine.cpp
    rimecandidate.cpp
    rimesession.cpp
    rimeaction.cpp
)

set(RIME_LINK_LIBRARIES
    Fcitx5::Core
    Fcitx5::Config
    ${RIME_TARGET}
    Fcitx5::Module::Notifications
    Pthread::Pthread
)

find_package(Fcitx5ModuleDBus QUIET)
if (Fcitx5ModuleDBus_FOUND)
    list(APPEND RIME_SOURCES rimeservice.cpp)
    list(APPEND RIME_LINK_LIBRARIES Fcitx5::Module::DBus)
else()
    add_definitions(-DFCITX_RIME_NO_DBUS)
endif()

add_library(rime MODULE ${RIME_SOURCES})
target_link_libraries(rime ${RIME_LINK_LIBRARIES})
install(TARGETS rime DESTINATION "${CMAKE_INSTALL_LIBDIR}/fcitx5")
fcitx5_translate_desktop_file(rime.conf.in rime.conf)
configure_file(rime-addon.conf.in.in rime-addon.conf.in)
fcitx5_translate_desktop_file("${CMAKE_CURRENT_BINARY_DIR}/rime-addon.conf.in" rime-addon.conf)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/rime.conf" DESTINATION "${FCITX_INSTALL_PKGDATADIR}/inputmethod" COMPONENT config)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/rime-addon.conf" RENAME rime.conf DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon" COMPONENT config)

