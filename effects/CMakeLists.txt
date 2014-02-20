set(kwin_effect_OWN_LIBS
    kwineffects
)

set(kwin_effect_KDE_LIBS
    KF5::ConfigCore
    KF5::ConfigWidgets
    KF5::GlobalAccel
    KF5::I18n
    KF5::WindowSystem
    KF5::Plasma # screenedge effect
    KF5::IconThemes
    KF5::Service
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
    XCB::XCB
    XCB::IMAGE
    XCB::XFIXES
)

if( KWIN_HAVE_XRENDER_COMPOSITING )
    set(kwin_effect_XCB_LIBS ${kwin_effect_XCB_LIBS} XCB::RENDER)
endif()

if(KWIN_BUILD_OPENGL)
    set(kwin_effect_OWN_LIBS ${kwin_effect_OWN_LIBS} kwinglutils)
elseif(KWIN_BUILD_OPENGLES)
    set(kwin_effect_OWN_LIBS ${kwin_effect_OWN_LIBS} kwinglesutils)
endif()

macro( KWIN4_ADD_EFFECT_BACKEND name )
    add_library( ${name} SHARED ${ARGN} )
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

    set_target_properties(kwin4_effect_${name} PROPERTIES VERSION 1.0.0 SOVERSION 1 )
    set_target_properties(kwin4_effect_${name} PROPERTIES OUTPUT_NAME ${KWIN_NAME}4_effect_${name})
    install(TARGETS kwin4_effect_${name} ${INSTALL_TARGETS_DEFAULT_ARGS} )

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
        KF5::CoreAddons
        KF5::Completion
        KF5::ConfigWidgets
        KF5::GlobalAccel
        KF5::I18n
        KF5::XmlGui
        )
    install( TARGETS kcm_kwin4_effect_${name} DESTINATION ${PLUGIN_INSTALL_DIR} )
endmacro()

# Install the KWin/Effect service type
install( FILES kwineffect.desktop DESTINATION ${SERVICETYPES_INSTALL_DIR} )

# Create initial variables
set( kwin4_effect_builtins_sources logging.cpp effect_builtins.cpp)
set( kwin4_effect_builtins_config_sources configs_builtins.cpp )
set( kwin4_effect_include_directories )

# scripted effects
add_subdirectory( dialogparent )
add_subdirectory( fade )
add_subdirectory( fadedesktop )
add_subdirectory( login )
add_subdirectory( maximize )
add_subdirectory( scalein )
add_subdirectory( translucency )

###############################################################################
# Built-in effects go here

# Common effects
include( dashboard/CMakeLists.txt )
include( desktopgrid/CMakeLists.txt )
include( diminactive/CMakeLists.txt )
include( dimscreen/CMakeLists.txt )
include( fallapart/CMakeLists.txt )
include( highlightwindow/CMakeLists.txt )
include( kscreen/CMakeLists.txt )
include( magiclamp/CMakeLists.txt )
include( minimizeanimation/CMakeLists.txt )
include( presentwindows/CMakeLists.txt )
include( resize/CMakeLists.txt )
include( screenedge/CMakeLists.txt )
include( showfps/CMakeLists.txt )
include( showpaint/CMakeLists.txt )
include( slide/CMakeLists.txt )
include( slideback/CMakeLists.txt )
include( slidingpopups/CMakeLists.txt )
include( thumbnailaside/CMakeLists.txt )
include( windowgeometry/CMakeLists.txt )
include( zoom/CMakeLists.txt )
include( logout/CMakeLists.txt )

# OpenGL-specific effects
include( blur/CMakeLists.txt )
include( backgroundcontrast/CMakeLists.txt )
include( coverswitch/CMakeLists.txt )
include( cube/CMakeLists.txt )
include( flipswitch/CMakeLists.txt )
include( glide/CMakeLists.txt )
include( invert/CMakeLists.txt )
include( lookingglass/CMakeLists.txt )
include( magnifier/CMakeLists.txt )
include( mouseclick/CMakeLists.txt )
include( mousemark/CMakeLists.txt )
include( screenshot/CMakeLists.txt )
include( sheet/CMakeLists.txt )
include( snaphelper/CMakeLists.txt )
include( startupfeedback/CMakeLists.txt )
include( trackmouse/CMakeLists.txt )
include( wobblywindows/CMakeLists.txt )

###############################################################################

# Add the builtins plugin
kwin4_add_effect( builtins ${kwin4_effect_builtins_sources} )
kwin4_add_effect_config( builtins ${kwin4_effect_builtins_config_sources} )
