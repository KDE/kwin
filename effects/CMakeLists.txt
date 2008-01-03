# Uncomment to have the test effects built
#add_subdirectory( test )


# Adds effect plugin with given name. Sources are given after the name
macro(KWIN4_ADD_EFFECT name)
    kde4_add_plugin(kwin4_effect_${name} ${ARGN})
    target_link_libraries(kwin4_effect_${name} kwineffects ${KDE4_KDEUI_LIBS})
    install(TARGETS kwin4_effect_${name} DESTINATION ${PLUGIN_INSTALL_DIR})
endmacro(KWIN4_ADD_EFFECT)

macro(KWIN4_ADD_EFFECT_CONFIG name)
    set(kwin4_effect_ui ) #empty
    set(kwin4_effect_src ) #empty
    foreach(file ${ARGN})
        if(file MATCHES \\.ui)
            set(kwin4_effect_ui ${kwin4_effect_ui} ${file})
        else(file MATCHES \\.ui)
            set(kwin4_effect_src ${kwin4_effect_src} ${file})
        endif(file MATCHES \\.ui)
    endforeach(file)
    kde4_add_ui_files(kwin4_effect_src ${kwin4_effect_ui})
    kde4_add_plugin(kcm_kwin4_effect_${name} ${kwin4_effect_src})
    target_link_libraries(kcm_kwin4_effect_${name} kwineffects ${KDE4_KDEUI_LIBS})
    install(TARGETS kcm_kwin4_effect_${name} DESTINATION ${PLUGIN_INSTALL_DIR})
endmacro(KWIN4_ADD_EFFECT_CONFIG)

include_directories(
    ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/lib
    )

install( FILES kwineffect.desktop DESTINATION ${SERVICETYPES_INSTALL_DIR})

### builtins - most important, ready-to-use effects
# sources
SET(kwin4_effect_builtins_sources
    boxswitch.cpp
    desktopgrid.cpp
    dialogparent.cpp
    diminactive.cpp
    fade.cpp
    fallapart.cpp
    login.cpp
    logout.cpp
    maketransparent.cpp
    minimizeanimation.cpp
    presentwindows.cpp
    scalein.cpp
    taskbarthumbnail.cpp
    thumbnailaside.cpp
    zoom.cpp
    )
# their .desktop files
install( FILES
    boxswitch.desktop
    desktopgrid.desktop
    dialogparent.desktop
    diminactive.desktop
    fade.desktop
    fallapart.desktop
    login.desktop
    logout.desktop
    maketransparent.desktop
    minimizeanimation.desktop
    presentwindows.desktop
    scalein.desktop
    taskbarthumbnail.desktop
    thumbnailaside.desktop
    zoom.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

# config modules
SET(kwin4_effect_builtins_config_sources
    desktopgrid_config.cpp
    diminactive_config.cpp
    diminactive_config.ui
    maketransparent_config.cpp
    presentwindows_config.cpp
    shadow_config.cpp
    thumbnailaside_config.cpp
    thumbnailaside_config.ui
    zoom_config.cpp
    configs_builtins.cpp)
install( FILES
    desktopgrid_config.desktop
    diminactive_config.desktop
    maketransparent_config.desktop
    presentwindows_config.desktop
    shadow_config.desktop
    thumbnailaside_config.desktop
    zoom_config.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

if(KWIN_HAVE_OPENGL_COMPOSITING)
    # opengl-based effects
    SET(kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
        blur.cpp
        explosioneffect.cpp
        invert.cpp
        lookingglass.cpp
        magnifier.cpp
        mousemark.cpp
        shadow.cpp
        sharpen.cpp
        trackmouse.cpp
        )
    install( FILES
        blur.desktop
        explosion.desktop
        invert.desktop
        lookingglass.desktop
        magnifier.desktop
        mousemark.desktop
        shadow.desktop
        sharpen.desktop
        trackmouse.desktop
        DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
    install( FILES
        data/trackmouse.png
        data/explosion.frag
        data/explosion.vert
        data/explosion-start.png
        data/explosion-end.png
        data/blur.frag
        data/blur.vert
        data/blur-render.frag
        data/blur-render.vert
        data/invert.frag
        data/invert.vert
        data/sharpen.frag
        data/sharpen.vert
        data/lookingglass.frag
        data/lookingglass.vert
        data/shadow-texture.png
        data/circle.png
        data/circle-edgy.png
        DESTINATION  ${DATA_INSTALL_DIR}/kwin )
    SET(kwin4_effect_builtins_config_sources ${kwin4_effect_builtins_config_sources}
        invert_config.cpp
        lookingglass_config.cpp
        lookingglass_config.ui
        magnifier_config.cpp
        magnifier_config.ui
        mousemark_config.cpp
        mousemark_config.ui
        sharpen_config.cpp
        trackmouse_config.cpp
        )
    install( FILES
        invert_config.desktop
        lookingglass_config.desktop
        magnifier_config.desktop
        mousemark_config.desktop
        sharpen_config.desktop
        trackmouse_config.desktop
        DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
endif(KWIN_HAVE_OPENGL_COMPOSITING)

# showfps, showpaint - need both xrender and opengl
if(KWIN_HAVE_OPENGL_COMPOSITING AND KWIN_HAVE_XRENDER_COMPOSITING)
    SET(kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
        showfps.cpp
        showpaint.cpp
        )
    install( FILES
        showfps.desktop
        showpaint.desktop
        DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
endif(KWIN_HAVE_OPENGL_COMPOSITING AND KWIN_HAVE_XRENDER_COMPOSITING)

# add the plugin
KWIN4_ADD_EFFECT(builtins ${kwin4_effect_builtins_sources})
KWIN4_ADD_EFFECT_CONFIG(builtins ${kwin4_effect_builtins_config_sources})
# link to xrender if necessary
# note that libkwineffects already links to opengl, so no need to add this here
if (KWIN_HAVE_XRENDER_COMPOSITING)
  target_link_libraries(kwin4_effect_builtins ${X11_Xrender_LIB})
endif (KWIN_HAVE_XRENDER_COMPOSITING)




### videocapture plugin
include(UsePkgConfig)
PKGCONFIG(libcaptury CAPTURY_INCLUDES CAPTURY_LINK_DIR CAPTURY_LDFLAGS CAPTURY_CFLAGS)
if( CAPTURY_LDFLAGS )
    SET( CAPTURY_FOUND TRUE )
endif( CAPTURY_LDFLAGS )

macro_log_feature(CAPTURY_FOUND "Captury framework" "libcaptury is a framework realtime video capturing for e.g. screen casts" "http://rm-rf.in/captury")

# HAVE_CAPTURY doesn't work for some reason, but it's not used now anyway
macro_bool_to_01( CAPTURY_FOUND HAVE_CAPTURY )
if( HAVE_CAPTURY )
    KWIN4_ADD_EFFECT(videorecord videorecord.cpp)
    KWIN4_ADD_EFFECT_CONFIG(videorecord videorecord_config.cpp)
    target_link_libraries(kwin4_effect_videorecord ${CAPTURY_LDFLAGS})
    install( FILES videorecord.desktop videorecord_config.desktop DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
endif( HAVE_CAPTURY )

