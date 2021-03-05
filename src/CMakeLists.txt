set(RIME_SOURCES
    rimestate.cpp
    rimeengine.cpp
    rimecandidate.cpp
)
add_library(rime MODULE ${RIME_SOURCES})
target_link_libraries(rime Fcitx5::Core Fcitx5::Config PkgConfig::Rime Fcitx5::Module::Notifications)
set_target_properties(rime PROPERTIES PREFIX "")
install(TARGETS rime DESTINATION "${CMAKE_INSTALL_LIBDIR}/fcitx5")
fcitx5_translate_desktop_file(rime.conf.in rime.conf)
configure_file(rime-addon.conf.in.in rime-addon.conf.in)
fcitx5_translate_desktop_file("${CMAKE_CURRENT_BINARY_DIR}/rime-addon.conf.in" rime-addon.conf)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/rime.conf" DESTINATION "${CMAKE_INSTALL_DATADIR}/fcitx5/inputmethod")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/rime-addon.conf" RENAME rime.conf DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon")

