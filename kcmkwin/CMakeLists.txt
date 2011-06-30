kde4_no_enable_final( kwinoptions )

add_subdirectory( kwinoptions )
add_subdirectory( kwindecoration )
add_subdirectory( kwinrules )
add_subdirectory( kwincompositing )
add_subdirectory( kwinscreenedges )
add_subdirectory( kwindesktop )

if( KWIN_BUILD_TABBOX )
add_subdirectory( kwintabbox )
endif( KWIN_BUILD_TABBOX )
