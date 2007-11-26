add_definitions (-DQT3_SUPPORT)

########### next target ###############

set(kwin_oxygen_SRCS
     lib/helper.cpp
     oxygenclient.cpp
     oxygenbutton.cpp
     oxygen.cpp
   )

kde4_add_plugin(kwin3_oxygen ${kwin_oxygen_SRCS})


target_link_libraries(kwin3_oxygen ${KDE4_KDEUI_LIBS} ${X11_Xrender_LIB} kdecorations)

install(TARGETS kwin3_oxygen  DESTINATION ${PLUGIN_INSTALL_DIR} )

########### install files ###############

install( FILES oxygenclient.desktop  DESTINATION  ${DATA_INSTALL_DIR}/kwin/ )

