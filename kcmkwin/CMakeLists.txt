remove_definitions(-DQT_NO_CAST_FROM_ASCII -DQT_STRICT_ITERATORS -DQT_NO_CAST_FROM_BYTEARRAY -DQT_NO_KEYWORDS)

add_subdirectory( kwinoptions )
add_subdirectory( kwindecoration )
add_subdirectory( kwinrules )
if(KWIN_BUILD_SCREENEDGES)
    add_subdirectory( kwinscreenedges )
endif()
add_subdirectory( kwinscripts )
add_subdirectory( kwindesktop )

if( KWIN_BUILD_TABBOX )
add_subdirectory( kwintabbox )
endif()
