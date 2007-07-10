########### next target ###############

set(kcm_kwineffects_PART_SRCS main.cpp )
kde4_add_plugin(kcm_kwineffects ${kcm_kwineffects_PART_SRCS})
target_link_libraries(kcm_kwineffects ${KDE4_KUTILS_LIBS})
install(TARGETS kcm_kwineffects  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############

install( FILES kwineffects.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )


