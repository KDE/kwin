set(kcm_kwin4_genericscripted_SRCS genericscriptedconfig.cpp)
qt5_add_dbus_interface(kcm_kwin4_genericscripted_SRCS ${kwin_effects_dbus_xml} kwineffects_interface)
add_library(kcm_kwin4_genericscripted MODULE ${kcm_kwin4_genericscripted_SRCS})
target_link_libraries(kcm_kwin4_genericscripted
    KF5::ConfigWidgets #KCModule
    KF5::I18n
    KF5::Service
    Qt5::DBus
    Qt5::UiTools
)
install(TARGETS kcm_kwin4_genericscripted DESTINATION ${PLUGIN_INSTALL_DIR}/kwin/effects/configs)
