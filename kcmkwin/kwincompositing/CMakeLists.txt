########### next target ###############

include_directories( ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin )

set(kcm_kwincompositing_PART_SRCS
    main.cpp
    ktimerdialog.cpp
    ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/compositingprefs.cpp
    ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/libkwineffects/kwinglobals.cpp
    ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/libkwineffects/kwinglplatform.cpp
    )
kde4_add_ui_files(kcm_kwincompositing_PART_SRCS main.ui)
set(kwin_xml ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/org.kde.KWin.xml)
set_source_files_properties(${kwin_xml} PROPERTIES INCLUDE "interface_util.h")
QT4_ADD_DBUS_INTERFACE(kcm_kwincompositing_PART_SRCS ${kwin_xml} kwin_interface)
kde4_add_plugin(kcm_kwincompositing ${kcm_kwincompositing_PART_SRCS})
target_link_libraries(kcm_kwincompositing ${KDE4_KCMUTILS_LIBS} ${KDE4_KDEUI_LIBS} ${X11_LIBRARIES})
install(TARGETS kcm_kwincompositing  DESTINATION ${PLUGIN_INSTALL_DIR} )

# CompositingPrefs uses OpenGL
if(OPENGL_FOUND AND NOT KWIN_HAVE_OPENGLES_COMPOSITING)
  target_link_libraries(kcm_kwincompositing kwinglutils ${OPENGL_gl_LIBRARY})
  set_target_properties(kcm_kwincompositing PROPERTIES COMPILE_FLAGS -DKWIN_HAVE_OPENGL)
  # -ldl used by OpenGL code
  find_library(DL_LIBRARY dl)
  if (DL_LIBRARY)
    target_link_libraries(kcm_kwincompositing ${DL_LIBRARY})
  endif(DL_LIBRARY)
endif(OPENGL_FOUND AND NOT KWIN_HAVE_OPENGLES_COMPOSITING)
if(KWIN_HAVE_OPENGLES_COMPOSITING)
  target_link_libraries(kcm_kwincompositing kwinglesutils ${OPENGLES_LIBRARIES} ${OPENGLES_EGL_LIBRARIES})
  set_target_properties(kcm_kwincompositing PROPERTIES COMPILE_FLAGS "-DKWIN_HAVE_OPENGL -DKWIN_HAVE_OPENGLES")
endif(KWIN_HAVE_OPENGLES_COMPOSITING)
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


