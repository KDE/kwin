kde4_no_enable_final( kwinoptions )

add_subdirectory( kwinoptions )
add_subdirectory( kwindecoration )
add_subdirectory( kwinrules )
if( NOT KWIN_BUILD_WITH_OPENGLES )
    # depends on GL functionality not yet ported to ES
    add_subdirectory( kwincompositing )
    add_subdirectory( kwinscreenedges )
endif( NOT KWIN_BUILD_WITH_OPENGLES )
add_subdirectory( kwindesktop )
add_subdirectory( kwintabbox )
