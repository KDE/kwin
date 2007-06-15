project(kstyle-oxygen)

########### next target ###############

set( oxygen_SRCS
     oxygen.cpp
     tileset.cpp
     stylehint.cpp
     sizefromcontents.cpp
     qsubcmetrics.cpp
     pixelmetric.cpp
     stdpix.cpp
     drawcomplexcontrol.cpp
     drawcontrol.cpp
     drawprimitive.cpp
     dynamicbrush.cpp
     oxrender.cpp
   )

qt4_add_resources(oxygen.qrc oxygen.qrc)

kde4_automoc( ${oxygen_SRCS} )
kde4_add_plugin(kstyle-oxygen ${oxygen_SRCS} )

# TODO: check if too much definitions.
target_link_libraries( kstyle-oxygen 
	${KDE4_KDECORE_LIBS} 
	${QT_QT3SUPPORT_LIBRARY} 
	${QT_QTOPENGL_LIBRARY}
	${KDE4_KDEFX_LIBS} 
	${X11_X11_LIB}
	${X11_Xfixes_LIB}
	${X11_Xcursor_LIB}
	${X11_Xinerama_LIB}
	${X11_Xrender_LIB}
	${X11_Xrandr_LIB}
	${X11_XTest_LIB}
	${X11_LIBRARIES}
#	freetype
#	fontconfig
	GLU GL
)

install( TARGETS kstyle-oxygen DESTINATION ${PLUGIN_INSTALL_DIR}/plugins/styles/ )

########### install files ###############

install( FILES oxygen.themerc DESTINATION  ${DATA_INSTALL_DIR}/kstyle/themes )

