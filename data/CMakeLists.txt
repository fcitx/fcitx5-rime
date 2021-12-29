install(DIRECTORY 48x48 scalable DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor"
        PATTERN .* EXCLUDE
        PATTERN *~ EXCLUDE)

install(FILES fcitx5.yaml DESTINATION "${RIME_DATA_DIR}")
