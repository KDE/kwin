include_directories(${KDEBASE_WORKSPACE_SOURCE_DIR}/libs/oxygen)
include_directories( ${KDE4_KDEUI_INCLUDES} )


########### next target ###############
set( oxygen_shadow_demo_SOURCES
    oxygenshadowdemodialog.cpp
    oxygenshadowdemowidget.cpp
    main.cpp)

kde4_add_ui_files(oxygen_shadow_demo_SOURCES
    ui/oxygenshadowdemo.ui
)

kde4_add_executable( oxygen-shadow-demo ${oxygen_shadow_demo_SOURCES} )

target_link_libraries( oxygen-shadow-demo
    ${KDE4_KDEUI_LIBS}
    ${X11_X11_LIB}
    ${X11_Xrender_LIB}
    ${KDE4_KIO_LIBS}
    oxygenstyle
  )

########### install files ###############
install (TARGETS oxygen-shadow-demo ${INSTALL_TARGETS_DEFAULT_ARGS} )
