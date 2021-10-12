# KI18N Translation Domain for this library
add_definitions(-DTRANSLATION_DOMAIN=\"kwin_effects\" -DEFFECT_BUILTINS)

include_directories(${KWin_SOURCE_DIR}/src) # for xcbutils.h

if (HAVE_ACCESSIBILITY)
    include_directories(${QACCESSIBILITYCLIENT_INCLUDE_DIR})
endif()

set(kwin_effect_OWN_LIBS
    kwineffects
)

set(kwin_effect_KDE_LIBS
    KF5::ConfigGui
    KF5::ConfigWidgets
    KF5::GlobalAccel
    KF5::I18n
    KF5::Notifications # screenshot effect
    KF5::Plasma # screenedge effect
    KF5::WindowSystem
    KF5::Service # utils / screenshot effect
    Plasma::KWaylandServer
)

if (HAVE_ACCESSIBILITY)
    set(kwin_effect_KDE_LIBS ${kwin_effect_KDE_LIBS} ${QACCESSIBILITYCLIENT_LIBRARY})
endif()

set(kwin_effect_QT_LIBS
    Qt::Concurrent
    Qt::DBus
    Qt::Quick
    Qt::X11Extras
)

set(kwin_effect_XLIB_LIBS
    ${X11_X11_LIB}
)

set(kwin_effect_XCB_LIBS
    XCB::IMAGE
    XCB::XCB
    XCB::XFIXES
)

set(kwin_effect_OWN_LIBS ${kwin_effect_OWN_LIBS} kwinglutils)

macro(KWIN4_ADD_EFFECT_BACKEND name type)
    add_library(${name} ${type} ${ARGN})
    target_link_libraries(${name} PRIVATE
        ${kwin_effect_KDE_LIBS}
        ${kwin_effect_OWN_LIBS}
        ${kwin_effect_QT_LIBS}
        ${kwin_effect_XCB_LIBS}
        ${kwin_effect_XLIB_LIBS}
    )
endmacro()

macro(KWIN4_ADD_EFFECT_MODULE name)
    kwin4_add_effect_backend(${name} OBJECT ${ARGN})
    target_compile_definitions(${name} PRIVATE QT_STATICPLUGIN)
endmacro()

# Install the KWin/Effect service type
install(FILES kwineffect.desktop DESTINATION ${KDE_INSTALL_KSERVICETYPES5DIR})

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
add_subdirectory(colorpicker)
add_subdirectory(desktopgrid)
add_subdirectory(diminactive)
add_subdirectory(fallapart)
add_subdirectory(highlightwindow)
add_subdirectory(kscreen)
add_subdirectory(screentransform)
add_subdirectory(magiclamp)
add_subdirectory(overview)
add_subdirectory(presentwindows)
add_subdirectory(resize)
add_subdirectory(screenedge)
add_subdirectory(showfps)
add_subdirectory(showpaint)
add_subdirectory(slide)
add_subdirectory(slideback)
add_subdirectory(slidingpopups)
add_subdirectory(thumbnailaside)
add_subdirectory(touchpoints)
add_subdirectory(windowgeometry)
add_subdirectory(zoom)

# OpenGL-specific effects
add_subdirectory(blur)
add_subdirectory(backgroundcontrast)
add_subdirectory(glide)
add_subdirectory(invert)
add_subdirectory(lookingglass)
add_subdirectory(magnifier)
add_subdirectory(mouseclick)
add_subdirectory(mousemark)
add_subdirectory(screenshot)
add_subdirectory(sheet)
add_subdirectory(snaphelper)
add_subdirectory(startupfeedback)
add_subdirectory(trackmouse)
add_subdirectory(wobblywindows)

###############################################################################

# Add the builtins plugin
set(kwin4_effect_builtins_sources
    logging.cpp
    ../service_utils.cpp
)

qt5_add_resources(kwin4_effect_builtins_sources shaders.qrc)

kwin4_add_effect_backend(kwin4_effect_builtins STATIC ${kwin4_effect_builtins_sources})
target_link_libraries(kwin4_effect_builtins PRIVATE
    kwin4_effect_blur
    kwin4_effect_colorpicker
    kwin4_effect_contrast
    kwin4_effect_desktopgrid
    kwin4_effect_diminactive
    kwin4_effect_fallapart
    kwin4_effect_glide
    kwin4_effect_highlightwindow
    kwin4_effect_invert
    kwin4_effect_kscreen
    kwin4_effect_lookingglass
    kwin4_effect_magiclamp
    kwin4_effect_magnifier
    kwin4_effect_mouseclick
    kwin4_effect_mousemark
    kwin4_effect_overview
    kwin4_effect_presentwindows
    kwin4_effect_resize
    kwin4_effect_screenedge
    kwin4_effect_screenshot
    kwin4_effect_screentransform
    kwin4_effect_sheet
    kwin4_effect_showfps
    kwin4_effect_showpaint
    kwin4_effect_slide
    kwin4_effect_slideback
    kwin4_effect_slidingpopups
    kwin4_effect_snaphelper
    kwin4_effect_startupfeedback
    kwin4_effect_thumbnailaside
    kwin4_effect_touchpoints
    kwin4_effect_trackmouse
    kwin4_effect_windowgeometry
    kwin4_effect_wobblywindows
    kwin4_effect_zoom
)
