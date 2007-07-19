########### next target ###############

set(kcm_kwincompositing_PART_SRCS main.cpp )
kde4_add_ui_files(kcm_kwincompositing_PART_SRCS main.ui)
kde4_add_plugin(kcm_kwincompositing ${kcm_kwincompositing_PART_SRCS})
target_link_libraries(kcm_kwincompositing ${KDE4_KUTILS_LIBS})
install(TARGETS kcm_kwincompositing  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############

install( FILES kwincompositing.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )


