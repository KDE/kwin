########### next target ###############

set(kwin_oxy_SRCS
     lib/helper.cpp
     oxygenclient.cpp
     oxygenbutton.cpp
     oxygen.cpp
     definitions.cpp
   )


kde4_add_plugin(kwin3_oxy ${kwin_oxy_SRCS})


target_link_libraries(kwin3_oxy ${KDE4_KDEUI_LIBS} ${X11_Xrender_LIB} kdecorations)

install(TARGETS kwin3_oxy  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############

install( FILES oxygenclient.desktop  DESTINATION  ${DATA_INSTALL_DIR}/kwin/ )


