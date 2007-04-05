
project(kwin)

add_definitions (-DQT3_SUPPORT)

add_subdirectory( lib ) 
add_subdirectory( killer ) 
add_subdirectory( kcmkwin ) 
add_subdirectory( pics ) 
add_subdirectory( clients ) 
add_subdirectory( data ) 

include_directories( ${CMAKE_CURRENT_BINARY_DIR}/lib ${CMAKE_CURRENT_SOURCE_DIR}/lib )

########### next target ###############

set(kwin_KDEINIT_SRCS
   workspace.cpp 
   client.cpp 
   placement.cpp 
   atoms.cpp 
   utils.cpp 
   layers.cpp 
   main.cpp 
   popupinfo.cpp 
   tabbox.cpp 
   options.cpp 
   plugins.cpp 
   events.cpp 
   killwindow.cpp 
   geometrytip.cpp 
   sm.cpp 
   group.cpp 
   bridge.cpp 
   manage.cpp 
   notifications.cpp 
   activation.cpp 
   useractions.cpp 
   geometry.cpp 
   rules.cpp )

qt4_add_dbus_adaptor( kwin_KDEINIT_SRCS org.kde.KWin.xml workspace.h KWin::Workspace )

kde4_automoc(${kwin_KDEINIT_SRCS})


kde4_add_kdeinit_executable( kwin ${kwin_KDEINIT_SRCS})

target_link_libraries(kdeinit_kwin  ${KDE4_KDEUI_LIBS} kdecorations ${X11_LIBRARIES} ${QT_QT3SUPPORT_LIBRARY} )

install(TARGETS kdeinit_kwin  DESTINATION ${LIB_INSTALL_DIR} )

target_link_libraries( kwin kdeinit_kwin )
install(TARGETS kwin DESTINATION ${BIN_INSTALL_DIR})

########### install files ###############

install( FILES kwin.kcfg  DESTINATION  ${KCFG_INSTALL_DIR} )
install( FILES kwin.notifyrc  DESTINATION  ${DATA_INSTALL_DIR}/kwin )
install( FILES org.kde.KWin.xml DESTINATION ${DBUS_INTERFACES_DIR} )

kde4_install_icons( ${ICON_INSTALL_DIR}  )


