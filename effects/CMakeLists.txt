# KI18N Translation Domain for this library
add_definitions(-DTRANSLATION_DOMAIN=\"kwin_effects\" -DEFFECT_BUILTINS)

include_directories(${KWin_SOURCE_DIR}) # for xcbutils.h

if (HAVE_ACCESSIBILITY)
    include_directories(${QACCESSIBILITYCLIENT_INCLUDE_DIR})
endif()

set(kwin_effect_OWN_LIBS
    kwineffects
)

if (KWIN_HAVE_XRENDER_COMPOSITING)
    set(kwin_effect_OWN_LIBS ${kwin_effect_OWN_LIBS} kwinxrenderutils)
endif()

set(kwin_effect_KDE_LIBS
    KF5::ConfigGui
    KF5::ConfigWidgets
    KF5::GlobalAccel
    KF5::I18n
    KF5::Notifications # screenshot effect
    KF5::Plasma # screenedge effect
    KF5::WindowSystem
    KF5::Service # utils / screenshot effect
)

if (HAVE_ACCESSIBILITY)
    set(kwin_effect_KDE_LIBS ${kwin_effect_KDE_LIBS} ${QACCESSIBILITYCLIENT_LIBRARY})
endif()

set(kwin_effect_QT_LIBS
    Qt5::Concurrent
    Qt5::DBus
    Qt5::Quick
    Qt5::X11Extras
)

set(kwin_effect_XLIB_LIBS
    ${X11_X11_LIB}
)

set(kwin_effect_XCB_LIBS
    XCB::IMAGE
    XCB::XCB
    XCB::XFIXES
)

if (KWIN_HAVE_XRENDER_COMPOSITING)
    set(kwin_effect_XCB_LIBS ${kwin_effect_XCB_LIBS} XCB::RENDER)
endif()

set(kwin_effect_OWN_LIBS ${kwin_effect_OWN_LIBS} kwinglutils)

macro(KWIN4_ADD_EFFECT_BACKEND name)
    add_library(${name} SHARED ${ARGN})
    target_link_libraries(${name} PRIVATE
        ${kwin_effect_KDE_LIBS}
        ${kwin_effect_OWN_LIBS}
        ${kwin_effect_QT_LIBS}
        ${kwin_effect_XCB_LIBS}
        ${kwin_effect_XLIB_LIBS}
    )
endmacro()

# Adds effect plugin with given name. Sources are given after the name
macro(KWIN4_ADD_EFFECT name)
    kwin4_add_effect_backend(kwin4_effect_${name} ${ARGN})

    set_target_properties(kwin4_effect_${name} PROPERTIES VERSION 1.0.0 SOVERSION 1)
    set_target_properties(kwin4_effect_${name} PROPERTIES OUTPUT_NAME ${KWIN_NAME}4_effect_${name})
    install(TARGETS kwin4_effect_${name} ${INSTALL_TARGETS_DEFAULT_ARGS})
endmacro()

# Install the KWin/Effect service type
install(FILES kwineffect.desktop DESTINATION ${SERVICETYPES_INSTALL_DIR})

# Create initial variables
set(kwin4_effect_include_directories)

set(kwin4_effect_builtins_sources
    blur/blur.cpp
    blur/blurshader.cpp
    colorpicker/colorpicker.cpp
    coverswitch/coverswitch.cpp
    cube/cube.cpp
    cube/cube_proxy.cpp
    cubeslide/cubeslide.cpp
    desktopgrid/desktopgrid.cpp
    diminactive/diminactive.cpp
    effect_builtins.cpp
    flipswitch/flipswitch.cpp
    glide/glide.cpp
    invert/invert.cpp
    logging.cpp
    lookingglass/lookingglass.cpp
    magiclamp/magiclamp.cpp
    magnifier/magnifier.cpp
    mouseclick/mouseclick.cpp
    mousemark/mousemark.cpp
    presentwindows/presentwindows.cpp
    presentwindows/presentwindows_proxy.cpp
    resize/resize.cpp
    showfps/showfps.cpp
    showpaint/showpaint.cpp
    slide/slide.cpp
    thumbnailaside/thumbnailaside.cpp
    touchpoints/touchpoints.cpp
    trackmouse/trackmouse.cpp
    windowgeometry/windowgeometry.cpp
    wobblywindows/wobblywindows.cpp
    zoom/zoom.cpp
    ../service_utils.cpp
)

if (HAVE_ACCESSIBILITY)
    set(kwin4_effect_builtins_sources
        zoom/accessibilityintegration.cpp
        ${kwin4_effect_builtins_sources}
    )
endif()

qt5_add_resources(kwin4_effect_builtins_sources shaders.qrc)

kconfig_add_kcfg_files(kwin4_effect_builtins_sources
    blur/blurconfig.kcfgc
    coverswitch/coverswitchconfig.kcfgc
    cube/cubeconfig.kcfgc
    cubeslide/cubeslideconfig.kcfgc
    desktopgrid/desktopgridconfig.kcfgc
    diminactive/diminactiveconfig.kcfgc
    fallapart/fallapartconfig.kcfgc
    flipswitch/flipswitchconfig.kcfgc
    glide/glideconfig.kcfgc
    lookingglass/lookingglassconfig.kcfgc
    magiclamp/magiclampconfig.kcfgc
    magnifier/magnifierconfig.kcfgc
    mouseclick/mouseclickconfig.kcfgc
    mousemark/mousemarkconfig.kcfgc
    presentwindows/presentwindowsconfig.kcfgc
    resize/resizeconfig.kcfgc
    showfps/showfpsconfig.kcfgc
    slide/slideconfig.kcfgc
    slidingpopups/slidingpopupsconfig.kcfgc
    thumbnailaside/thumbnailasideconfig.kcfgc
    trackmouse/trackmouseconfig.kcfgc
    windowgeometry/windowgeometryconfig.kcfgc
    wobblywindows/wobblywindowsconfig.kcfgc
    zoom/zoomconfig.kcfgc
)

# scripted effects
function(install_scripted_effect name)
    kpackage_install_package(${name}/package kwin4_effect_${name} effects kwin)

    # necessary so tests are found without installing
    file(COPY ${name}/package/contents ${name}/package/metadata.desktop DESTINATION ${CMAKE_BINARY_DIR}/bin/kwin/effects/kwin4_effect_${name})
endfunction()
install_scripted_effect(dialogparent)
install_scripted_effect(dimscreen)
install_scripted_effect(eyeonscreen)
install_scripted_effect(fade)
install_scripted_effect(fadedesktop)
install_scripted_effect(fadingpopups)
install_scripted_effect(frozenapp)
install_scripted_effect(fullscreen)
install_scripted_effect(login)
install_scripted_effect(logout)
install_scripted_effect(maximize)
install_scripted_effect(morphingpopups)
install_scripted_effect(scale)
install_scripted_effect(squash)
install_scripted_effect(translucency)
install_scripted_effect(windowaperture)
install_scripted_effect(sessionquit)

###############################################################################
# Built-in effects go here

# Common effects
add_subdirectory(desktopgrid)
add_subdirectory(diminactive)
include(fallapart/CMakeLists.txt)
include(highlightwindow/CMakeLists.txt)
include(kscreen/CMakeLists.txt)
add_subdirectory(magiclamp)
add_subdirectory(presentwindows)
add_subdirectory(resize)
include(screenedge/CMakeLists.txt)
add_subdirectory(showfps)
add_subdirectory(showpaint)
add_subdirectory(slide)
include(slideback/CMakeLists.txt)
include(slidingpopups/CMakeLists.txt)
add_subdirectory(thumbnailaside)
add_subdirectory(windowgeometry)
add_subdirectory(zoom)

# OpenGL-specific effects
add_subdirectory(blur)
include(backgroundcontrast/CMakeLists.txt)
add_subdirectory(coverswitch)
add_subdirectory(cube)
add_subdirectory(cubeslide)
add_subdirectory(flipswitch)
add_subdirectory(glide)
add_subdirectory(invert)
add_subdirectory(lookingglass)
add_subdirectory(magnifier)
add_subdirectory(mouseclick)
add_subdirectory(mousemark)
include(screenshot/CMakeLists.txt)
include(sheet/CMakeLists.txt)
include(snaphelper/CMakeLists.txt)
include(startupfeedback/CMakeLists.txt)
add_subdirectory(trackmouse)
add_subdirectory(wobblywindows)

###############################################################################

# Add the builtins plugin
kwin4_add_effect(builtins ${kwin4_effect_builtins_sources})
