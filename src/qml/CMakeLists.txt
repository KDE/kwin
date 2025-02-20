install(DIRECTORY outline/plasma DESTINATION ${KDE_INSTALL_DATADIR}/kwin/outline)
install(DIRECTORY onscreennotification/plasma DESTINATION ${KDE_INSTALL_DATADIR}/kwin/onscreennotification)
install(DIRECTORY frames/plasma DESTINATION ${KDE_INSTALL_DATADIR}/kwin/frames)

ecm_add_qml_module(kwinplugin URI org.kde.kwin VERSION 3.0 GENERATE_PLUGIN_SOURCE)
ecm_target_qml_sources(kwinplugin SOURCES DBusCall.qml)
ecm_finalize_qml_module(kwinplugin DESTINATION ${KDE_INSTALL_QMLDIR})
