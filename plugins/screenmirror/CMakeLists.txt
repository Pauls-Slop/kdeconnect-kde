kdeconnect_add_plugin(kdeconnect_screenmirror SOURCES screenmirrorplugin.cpp adbsession.cpp adbtools.cpp)
target_link_libraries(kdeconnect_screenmirror
    kdeconnectcore
    Qt::Core
    Qt::Network
    Qt::DBus
    KF6::I18n
)

install(FILES "kdeconnect_screenmirror_config.qml" DESTINATION ${KDE_INSTALL_DATADIR}/kdeconnect)
