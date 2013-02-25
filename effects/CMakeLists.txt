# the factory macros cause errors
kde4_no_enable_final(kwineffects)

macro( KWIN4_ADD_EFFECT_BACKEND name )
    kde4_add_plugin( ${name} ${ARGN} )
    target_link_libraries( ${name} kwineffects ${KDE4_KDEUI_LIBS} ${KDE4_PLASMA_LIBS} ${X11_Xfixes_LIB} ${X11_Xcursor_LIB} ${X11_LIBRARIES} ${XCB_XCB_LIBRARIES} ${XCB_IMAGE_LIBRARIES} ${X11_XCB_LIBRARIES}
    ${XCB_XFIXES_LIBRARIES})
endmacro()

# Adds effect plugin with given name. Sources are given after the name
macro( KWIN4_ADD_EFFECT name )
    if(OPENGL_FOUND OR NOT(OPENGL_FOUND AND OPENGLES_FOUND))
        # OpenGL or neither OpenGL nor OpenGL ES - default set
        KWIN4_ADD_EFFECT_BACKEND(kwin4_effect_${name} ${ARGN})
        if(OPENGL_FOUND)
            target_link_libraries(kwin4_effect_${name} kwinglutils)
            if( KWIN_BUILD_OPENGL_1_COMPOSITING )
                set_target_properties(kwin4_effect_${name} PROPERTIES COMPILE_FLAGS -DKWIN_HAVE_OPENGL_1)
            endif()
        elseif(OPENGLES_FOUND)
            target_link_libraries(kwin4_effect_${name} kwinglesutils)
            set_target_properties(kwin4_effect_${name} PROPERTIES COMPILE_FLAGS "-DKWIN_HAVE_OPENGLES")
        endif()
        set_target_properties(kwin4_effect_${name} PROPERTIES OUTPUT_NAME ${KWIN_NAME}4_effect_${name})
        install( TARGETS kwin4_effect_${name} DESTINATION ${PLUGIN_INSTALL_DIR} )
    endif()


    if(OPENGLES_FOUND)
        KWIN4_ADD_EFFECT_BACKEND(kwin4_effect_gles_${name} ${ARGN})
        # OpenGL ES gets into a different library
        target_link_libraries(kwin4_effect_gles_${name} kwinglesutils)
        set_target_properties(kwin4_effect_gles_${name} PROPERTIES COMPILE_FLAGS "-DKWIN_HAVE_OPENGLES")
        set_target_properties(kwin4_effect_gles_${name} PROPERTIES OUTPUT_NAME ${KWIN_NAME}4_effect_gles_${name})
        install( TARGETS kwin4_effect_gles_${name} DESTINATION ${PLUGIN_INSTALL_DIR} )
    endif()
endmacro()

macro( KWIN4_ADD_EFFECT_CONFIG name )
    set( kwin4_effect_ui ) # Initially empty
    set( kwin4_effect_src ) # Initially empty

    foreach( file ${ARGN} )
        if( file MATCHES \\.ui )
            set( kwin4_effect_ui ${kwin4_effect_ui} ${file} )
        else( file MATCHES \\.ui )
            set( kwin4_effect_src ${kwin4_effect_src} ${file} )
        endif()
    endforeach()

    kde4_add_ui_files( kwin4_effect_src ${kwin4_effect_ui} )
    kde4_add_plugin( kcm_kwin4_effect_${name} ${kwin4_effect_src} )
    if(OPENGLES_FOUND)
        set_target_properties(kcm_kwin4_effect_${name} PROPERTIES COMPILE_FLAGS "-DKWIN_HAVE_OPENGLES")
    endif()
    target_link_libraries( kcm_kwin4_effect_${name} kwineffects ${KDE4_KIO_LIBS} ${KDE4_KDEUI_LIBS} )
    install( TARGETS kcm_kwin4_effect_${name} DESTINATION ${PLUGIN_INSTALL_DIR} )
endmacro()

macro( KWIN4_EFFECT_LINK_XRENDER name )
    if( KWIN_HAVE_XRENDER_COMPOSITING )
        target_link_libraries( kwin4_effect_${name} ${X11_Xrender_LIB} ${XCB_RENDER_LIBRARIES})

        # if building for OpenGL and OpenGL ES we have two targets
        # TODO: if building for OpenGL ES we should not build XRender support
        if(OPENGLES_FOUND)
            target_link_libraries( kwin4_effect_gles_${name} ${X11_Xrender_LIB} ${XCB_RENDER_LIBRARIES})
        endif()
    endif()
endmacro()

# Install the KWin/Effect service type
install( FILES kwineffect.desktop DESTINATION ${SERVICETYPES_INSTALL_DIR} )

# Create initial variables
set( kwin4_effect_builtins_sources )
if( NOT KWIN_MOBILE_EFFECTS )
    set( kwin4_effect_builtins_config_sources configs_builtins.cpp )
endif()
set( kwin4_effect_include_directories )

# scripted effects
add_subdirectory( fade )
add_subdirectory( login )

# scripted effects only relevant to desktop
if( NOT KWIN_MOBILE_EFFECTS )
    add_subdirectory( fadedesktop )
    add_subdirectory( maximize )
endif()

###############################################################################
# Built-in effects go here

# Common effects
include( dialogparent/CMakeLists.txt )
include( outline/CMakeLists.txt )
include( presentwindows/CMakeLists.txt )
include( screenedge/CMakeLists.txt )
include( slidingpopups/CMakeLists.txt )
include( taskbarthumbnail/CMakeLists.txt )

# Common effects only relevant to desktop
if( NOT KWIN_MOBILE_EFFECTS )
    include( boxswitch/CMakeLists.txt )
    include( dashboard/CMakeLists.txt )
    include( desktopgrid/CMakeLists.txt )
    include( diminactive/CMakeLists.txt )
    include( dimscreen/CMakeLists.txt )
    include( fallapart/CMakeLists.txt )
    include( highlightwindow/CMakeLists.txt )
    include( magiclamp/CMakeLists.txt )
    include( translucency/CMakeLists.txt )
    include( minimizeanimation/CMakeLists.txt )
    include( resize/CMakeLists.txt )
    include( scalein/CMakeLists.txt )
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
    include( explosion/CMakeLists.txt )
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
KWIN4_ADD_EFFECT( builtins ${kwin4_effect_builtins_sources} )
if( NOT KWIN_MOBILE_EFFECTS )
    KWIN4_ADD_EFFECT_CONFIG( builtins ${kwin4_effect_builtins_config_sources} )
endif()
KWIN4_EFFECT_LINK_XRENDER( builtins )
