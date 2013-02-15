set(kcm_kwin4_genericscripted_SRCS genericscriptedconfig.cpp)
kde4_add_plugin( kcm_kwin4_genericscripted ${kcm_kwin4_genericscripted_SRCS} )
target_link_libraries( kcm_kwin4_genericscripted ${KDE4_KIO_LIBS} ${KDE4_KDEUI_LIBS} ${KDE4_PLASMA_LIBS} ${QT_QTUITOOLS_LIBRARY} )
install( TARGETS kcm_kwin4_genericscripted DESTINATION ${PLUGIN_INSTALL_DIR} )
