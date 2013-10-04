set(kcm_kwin4_genericscripted_SRCS genericscriptedconfig.cpp)
kde4_add_plugin( kcm_kwin4_genericscripted ${kcm_kwin4_genericscripted_SRCS} )
target_link_libraries( kcm_kwin4_genericscripted
    KF5::KConfigWidgets #KCModule
    KF5::KDeclarative
    KF5::KI18n
    KF5::KService
    KF5::plasma
    Qt5::DBus
    Qt5::UiTools )
install( TARGETS kcm_kwin4_genericscripted DESTINATION ${PLUGIN_INSTALL_DIR} )
