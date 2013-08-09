set(kcm_kwin4_genericscripted_SRCS genericscriptedconfig.cpp)
kde4_add_plugin( kcm_kwin4_genericscripted ${kcm_kwin4_genericscripted_SRCS} )
target_link_libraries( kcm_kwin4_genericscripted
    ${KDE4Support_LIBRARIES} # KStandardDirs
    KF5::KConfigWidgets #KCModule
    KF5::KService
    KF5::plasma
    ${Qt5UiTools_LIBRARIES} )
install( TARGETS kcm_kwin4_genericscripted DESTINATION ${PLUGIN_INSTALL_DIR} )
