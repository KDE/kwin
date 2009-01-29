
add_subdirectory( decobenchmark ) 



########### next target ###############

set(test_gravity_SRCS test_gravity.cpp )

kde4_add_executable(test_gravity ${test_gravity_SRCS})

target_link_libraries(test_gravity  ${KDE4_KDECORE_LIBS} )

########### next target ###############

set(show_icons_SRCS show_icons.cpp )

kde4_add_executable(show_icons ${show_icons_SRCS})

target_link_libraries(show_icons ${KDE4_KDEUI_LIBS} )
