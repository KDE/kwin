add_definitions (-DQT3_SUPPORT -DQT3_SUPPORT_WARNINGS)

add_subdirectory( config ) 

########### next target ###############

set(kwin_ozone_SRCS
     lib/helper.cpp
     oxygenclient.cpp
     oxygenbutton.cpp
     oxygen.cpp
   )

kde4_add_plugin(kwin3_ozone ${kwin_ozone_SRCS})


target_link_libraries(kwin3_ozone ${KDE4_KDEUI_LIBS} ${X11_Xrender_LIB} kdecorations)

install(TARGETS kwin3_ozone  DESTINATION ${PLUGIN_INSTALL_DIR} )

########### install files ###############

install( FILES ozoneclient.desktop  DESTINATION  ${DATA_INSTALL_DIR}/kwin/ )

