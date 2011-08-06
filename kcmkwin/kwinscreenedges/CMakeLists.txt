include_directories( ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin )

set(
    kcm_kwinscreenedges_PART_SRCS
    main.cpp
    monitor.cpp
    ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/compositingprefs.cpp
    ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/libkwineffects/kwinglobals.cpp
    ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/libkwineffects/kwinglplatform.cpp
    )
kde4_add_ui_files( kcm_kwinscreenedges_PART_SRCS main.ui )
kde4_add_plugin( kcm_kwinscreenedges ${kcm_kwinscreenedges_PART_SRCS} )
target_link_libraries( kcm_kwinscreenedges ${X11_LIBRARIES} kworkspace ${KDE4_PLASMA_LIBS})
install( TARGETS kcm_kwinscreenedges  DESTINATION ${PLUGIN_INSTALL_DIR} )

# CompositingPrefs uses OpenGL
if( OPENGL_FOUND AND NOT KWIN_HAVE_OPENGLES_COMPOSITING )
  target_link_libraries( kcm_kwinscreenedges kwinglutils ${OPENGL_gl_LIBRARY} )
  set_target_properties(kcm_kwinscreenedges PROPERTIES COMPILE_FLAGS -DKWIN_HAVE_OPENGL)
# -ldl used by OpenGL code
  find_library( DL_LIBRARY dl )
  if( DL_LIBRARY )
    target_link_libraries( kcm_kwinscreenedges ${DL_LIBRARY} )
  endif( DL_LIBRARY )
endif( OPENGL_FOUND AND NOT KWIN_HAVE_OPENGLES_COMPOSITING )
if(KWIN_HAVE_OPENGLES_COMPOSITING)
  target_link_libraries(kcm_kwinscreenedges kwinglesutils ${OPENGLES_LIBRARIES})
  set_target_properties(kcm_kwinscreenedges PROPERTIES COMPILE_FLAGS "-DKWIN_HAVE_OPENGL -DKWIN_HAVE_OPENGLES")
endif(KWIN_HAVE_OPENGLES_COMPOSITING)
if( X11_Xrender_FOUND )
  target_link_libraries( kcm_kwinscreenedges ${X11_Xrender_LIB} )
endif( X11_Xrender_FOUND )
if( X11_Xrandr_FOUND )
  target_link_libraries( kcm_kwinscreenedges ${X11_Xrandr_LIB} )
endif( X11_Xrandr_FOUND )
if( X11_Xcomposite_FOUND )
  target_link_libraries( kcm_kwinscreenedges ${X11_Xcomposite_LIB} )
endif( X11_Xcomposite_FOUND )
if( X11_Xdamage_FOUND )
  target_link_libraries( kcm_kwinscreenedges ${X11_Xdamage_LIB} )
endif( X11_Xdamage_FOUND )
if( X11_Xfixes_FOUND )
  target_link_libraries( kcm_kwinscreenedges ${X11_Xfixes_LIB} )
endif( X11_Xfixes_FOUND )

install( FILES kwinscreenedges.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )
