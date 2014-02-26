set(kcm_kwin4_genericscripted_SRCS genericscriptedconfig.cpp)
kde4_add_plugin( kcm_kwin4_genericscripted ${kcm_kwin4_genericscripted_SRCS} )
target_link_libraries( kcm_kwin4_genericscripted
    KF5::ConfigWidgets #KCModule
    KF5::I18n
    KF5::Service
    KF5::Plasma
    Qt5::DBus
    Qt5::UiTools )
install( TARGETS kcm_kwin4_genericscripted DESTINATION ${PLUGIN_INSTALL_DIR} )
