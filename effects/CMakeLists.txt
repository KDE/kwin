set(kwin_effect_OWN_LIBS
    kwineffects
)

set(kwin_effect_KDE_LIBS
    KF5::KDE4Support
    KF5::KConfigCore
    KF5::KConfigWidgets
    KF5::KI18n
    KF5::KWindowSystem
    KF5::Plasma # screenedge effect
    KF5::KIconThemes
    KF5::KService
    KF5::XmlGui
)

set(kwin_effect_QT_LIBS
    Qt5::Concurrent
    Qt5::DBus
    Qt5::Quick
    Qt5::X11Extras
)

set(kwin_effect_XLIB_LIBS
    ${X11_X11_LIB}
    ${X11_Xcursor_LIB}
)

set(kwin_effect_XCB_LIBS
    ${XCB_XCB_LIBRARY}
    ${XCB_IMAGE_LIBRARY}
    ${XCB_XFIXES_LIBRARY}
)

if( KWIN_HAVE_XRENDER_COMPOSITING )
    set(kwin_effect_XCB_LIBS ${kwin_effect_XCB_LIBS} ${XCB_RENDER_LIBRARIES})
endif()

if(KWIN_BUILD_OPENGL)
    set(kwin_effect_OWN_LIBS ${kwin_effect_OWN_LIBS} kwinglutils)
elseif(KWIN_BUILD_OPENGLES)
    set(kwin_effect_OWN_LIBS ${kwin_effect_OWN_LIBS} kwinglesutils)
endif()

macro( KWIN4_ADD_EFFECT_BACKEND name )
    add_library( ${name} MODULE ${ARGN} )
    target_link_libraries( ${name} ${kwin_effect_OWN_LIBS} ${kwin_effect_KDE_LIBS} ${kwin_effect_QT_LIBS} ${kwin_effect_XLIB_LIBS} ${kwin_effect_XCB_LIBS})
endmacro()

# Adds effect plugin with given name. Sources are given after the name
macro( KWIN4_ADD_EFFECT name )
    kwin4_add_effect_backend(kwin4_effect_${name} ${ARGN})
    if(KWIN_BUILD_OPENGL)
        if( KWIN_BUILD_OPENGL_1_COMPOSITING )
            set_target_properties(kwin4_effect_${name} PROPERTIES COMPILE_FLAGS -DKWIN_HAVE_OPENGL_1)
        endif()
    elseif(KWIN_BUILD_OPENGLES)
        set_target_properties(kwin4_effect_${name} PROPERTIES COMPILE_FLAGS "-DKWIN_HAVE_OPENGLES")
    endif()
    set_target_properties(kwin4_effect_${name} PROPERTIES OUTPUT_NAME ${KWIN_NAME}4_effect_${name})
    install( TARGETS kwin4_effect_${name} DESTINATION ${PLUGIN_INSTALL_DIR} )

endmacro()

macro( KWIN4_ADD_EFFECT_CONFIG name )
    set( kwin4_effect_ui ) # Initially empty
    set( kwin4_effect_src ) # Initially empty

    foreach( file ${ARGN} )
        if( file MATCHES \\.ui )
            set( kwin4_effect_ui ${kwin4_effect_ui} ${file} )
        else()
            set( kwin4_effect_src ${kwin4_effect_src} ${file} )
        endif()
    endforeach()

    kde4_add_ui_files( kwin4_effect_src ${kwin4_effect_ui} )
    add_library( kcm_kwin4_effect_${name} MODULE ${kwin4_effect_src} )
    if(OPENGLES_FOUND)
        set_target_properties(kcm_kwin4_effect_${name} PROPERTIES COMPILE_FLAGS "-DKWIN_HAVE_OPENGLES")
    endif()
    target_link_libraries( kcm_kwin4_effect_${name}
        kwineffects
        Qt5::Widgets
        Qt5::Quick
        KF5::KIOWidgets # KUrlRequester
        KF5::KCoreAddons
        KF5::KCompletion
        KF5::KConfigWidgets
        KF5::KI18n
        KF5::XmlGui
        )
    install( TARGETS kcm_kwin4_effect_${name} DESTINATION ${PLUGIN_INSTALL_DIR} )
endmacro()

# Install the KWin/Effect service type
install( FILES kwineffect.desktop DESTINATION ${SERVICETYPES_INSTALL_DIR} )

# Create initial variables
set( kwin4_effect_builtins_sources logging.cpp)
if( NOT KWIN_MOBILE_EFFECTS )
    set( kwin4_effect_builtins_config_sources configs_builtins.cpp )
endif()
set( kwin4_effect_include_directories )

# scripted effects
add_subdirectory( dialogparent )
add_subdirectory( fade )
add_subdirectory( login )

# scripted effects only relevant to desktop
if( NOT KWIN_MOBILE_EFFECTS )
    add_subdirectory( fadedesktop )
    add_subdirectory( maximize )
    add_subdirectory( scalein )
    add_subdirectory( translucency )
endif()

###############################################################################
# Built-in effects go here

# Common effects
include( kscreen/CMakeLists.txt )
include( presentwindows/CMakeLists.txt )
include( screenedge/CMakeLists.txt )
include( slidingpopups/CMakeLists.txt )
include( taskbarthumbnail/CMakeLists.txt )

# Common effects only relevant to desktop
if( NOT KWIN_MOBILE_EFFECTS )
    include( dashboard/CMakeLists.txt )
    include( desktopgrid/CMakeLists.txt )
    include( diminactive/CMakeLists.txt )
    include( dimscreen/CMakeLists.txt )
    include( fallapart/CMakeLists.txt )
    include( highlightwindow/CMakeLists.txt )
    include( magiclamp/CMakeLists.txt )
    include( minimizeanimation/CMakeLists.txt )
    include( resize/CMakeLists.txt )
    include( showfps/CMakeLists.txt )
    include( showpaint/CMakeLists.txt )
    include( slide/CMakeLists.txt )
    include( slideback/CMakeLists.txt )
    include( thumbnailaside/CMakeLists.txt )
    include( windowgeometry/CMakeLists.txt )
    include( zoom/CMakeLists.txt )
    include( logout/CMakeLists.txt )
endif()

# OpenGL-specific effects
include( blur/CMakeLists.txt )
include( screenshot/CMakeLists.txt )
# OpenGL-specific effects for desktop
if( NOT KWIN_MOBILE_EFFECTS )
    include( coverswitch/CMakeLists.txt )
    include( cube/CMakeLists.txt )
    include( flipswitch/CMakeLists.txt )
    include( glide/CMakeLists.txt )
    include( invert/CMakeLists.txt )
    include( lookingglass/CMakeLists.txt )
    include( magnifier/CMakeLists.txt )
    include( mouseclick/CMakeLists.txt )
    include( mousemark/CMakeLists.txt )
    include( sheet/CMakeLists.txt )
    include( snaphelper/CMakeLists.txt )
    include( startupfeedback/CMakeLists.txt )
    include( trackmouse/CMakeLists.txt )
    include( wobblywindows/CMakeLists.txt )
endif()

###############################################################################

# Add the builtins plugin
kwin4_add_effect( builtins ${kwin4_effect_builtins_sources} )
if( NOT KWIN_MOBILE_EFFECTS )
    kwin4_add_effect_config( builtins ${kwin4_effect_builtins_config_sources} )
endif()
