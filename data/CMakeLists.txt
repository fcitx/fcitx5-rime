
# with prefix
foreach(svg rime rime-deploy rime-sync)
install(FILES ${svg}.png RENAME ${svg}.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/48x48/apps)
install(FILES ${svg}.svg RENAME ${svg}.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/status)
endforeach()

# without prefix
foreach(svg rime-im rime-latin rime-disable)
install(FILES ${svg}.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/48x48/apps)
install(FILES ${svg}.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/status)
endforeach()
