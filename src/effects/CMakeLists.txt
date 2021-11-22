# KI18N Translation Domain for this library
add_definitions(-DTRANSLATION_DOMAIN=\"kwin_effects\" -DEFFECT_BUILTINS)

include_directories(${KWin_SOURCE_DIR}/src) # for xcbutils.h

add_subdirectory(private)

if (CMAKE_CROSSCOMPILING)
    if (NOT KWIN_HOST_TOOLING)
        message(FATAL_ERROR "Please provide the path to a native build and pass -DKWIN_HOST_TOOLING=path")
    endif()
    find_file(KSEM_EXECUTABLE StripEffectMetadataTargets.cmake
              PATHS ${KWIN_HOST_TOOLING}
              NO_DEFAULT_PATH
              NO_CMAKE_FIND_ROOT_PATH)
    include(${KSEM_EXECUTABLE})
else()
    add_executable(kwin-strip-effect-metadata strip-effect-metadata.cpp)
    target_link_libraries(kwin-strip-effect-metadata Qt5::Core)
    # Export the kwin-strip-effect-metadata target to a file, so it can be used by another build.
    export(TARGETS kwin-strip-effect-metadata FILE ${CMAKE_BINARY_DIR}/StripEffectMetadataTargets.cmake)
endif()

function (kwin_strip_builtin_effect_metadata target metadata)
    set(stripped_metadata "${CMAKE_CURRENT_BINARY_DIR}/${metadata}.stripped")

    set(command kwin-strip-effect-metadata --source=${metadata} --output=${stripped_metadata})
    add_custom_command(
        OUTPUT ${stripped_metadata}
        COMMAND ${command}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS ${metadata}
        COMMENT "Stripping ${metadata}..."
    )
    set_property(TARGET ${target} APPEND PROPERTY AUTOGEN_TARGET_DEPENDS ${stripped_metadata})
endfunction()

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

macro(KWIN4_ADD_EFFECT_MODULE name)
    kcoreaddons_add_plugin(${name} STATIC SOURCES ${ARGN} INSTALL_NAMESPACE "kwin/effects/plugins")
    target_link_libraries(${name} PRIVATE
        ${kwin_effect_KDE_LIBS}
        ${kwin_effect_OWN_LIBS}
        ${kwin_effect_QT_LIBS}
        ${kwin_effect_XCB_LIBS}
        ${kwin_effect_XLIB_LIBS}
    )
    kwin_strip_builtin_effect_metadata(${name} metadata.json)
    install(FILES metadata.json DESTINATION ${KDE_INSTALL_DATADIR}/${KWIN_NAME}/builtin-effects/${name}/)
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
add_subdirectory(windowview)
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

add_library(kwin4_effect_builtins STATIC ${kwin4_effect_builtins_sources})
kcoreaddons_target_static_plugins(kwin4_effect_builtins "kwin/effects/plugins" LINK_OPTION "PRIVATE")
target_link_libraries(kwin4_effect_builtins PRIVATE
    ${kwin_effect_KDE_LIBS}
    ${kwin_effect_OWN_LIBS}
    ${kwin_effect_QT_LIBS}
    ${kwin_effect_XCB_LIBS}
    ${kwin_effect_XLIB_LIBS}
)
