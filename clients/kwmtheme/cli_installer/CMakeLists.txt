


########### next target ###############

set(kwmtheme_SRCS main.cpp )


kde4_add_executable(kwmtheme ${kwmtheme_SRCS})

target_link_libraries(kwmtheme  ${KDE4_KDECORE_LIBS} )

install(TARGETS kwmtheme ${INSTALL_TARGETS_DEFAULT_ARGS})

