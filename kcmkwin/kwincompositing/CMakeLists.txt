########### next target ###############

include_directories( ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin )

set(kcm_kwincompositing_PART_SRCS
    advanced.cpp
    main.cpp
    ktimerdialog.cpp
    ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/compositingprefs.cpp
    ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/lib/kwinglobals.cpp
    )
kde4_add_ui_files(kcm_kwincompositing_PART_SRCS advanced.ui main.ui)
kde4_add_plugin(kcm_kwincompositing ${kcm_kwincompositing_PART_SRCS})
target_link_libraries(kcm_kwincompositing ${KDE4_KUTILS_LIBS} ${KDE4_KDEUI_LIBS} ${X11_LIBRARIES})
install(TARGETS kcm_kwincompositing  DESTINATION ${PLUGIN_INSTALL_DIR} )

# CompositingPrefs uses OpenGL
if(OPENGL_FOUND)
  target_link_libraries(kcm_kwincompositing ${OPENGL_gl_LIBRARY})
# -ldl used by OpenGL code
  find_library(DL_LIBRARY dl)
  if (DL_LIBRARY)
    target_link_libraries(kcm_kwincompositing ${DL_LIBRARY})
  endif(DL_LIBRARY)
endif(OPENGL_FOUND)
if (X11_Xrender_FOUND)
  target_link_libraries(kcm_kwincompositing ${X11_Xrender_LIB})
endif (X11_Xrender_FOUND)
if (X11_Xrandr_FOUND)
  target_link_libraries(kcm_kwincompositing ${X11_Xrandr_LIB})
endif (X11_Xrandr_FOUND)
if (X11_Xcomposite_FOUND)
  target_link_libraries(kcm_kwincompositing ${X11_Xcomposite_LIB})
endif (X11_Xcomposite_FOUND)
if (X11_Xdamage_FOUND)
  target_link_libraries(kcm_kwincompositing ${X11_Xdamage_LIB})
endif (X11_Xdamage_FOUND)
if (X11_Xfixes_FOUND)
  target_link_libraries(kcm_kwincompositing ${X11_Xfixes_LIB})
endif (X11_Xfixes_FOUND)


########### install files ###############

install( FILES kwincompositing.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )


