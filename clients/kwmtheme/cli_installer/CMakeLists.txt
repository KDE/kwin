


########### next target ###############

set(kwmtheme_SRCS main.cpp )

kde4_automoc(kwmtheme ${kwmtheme_SRCS})

kde4_add_executable(kwmtheme ${kwmtheme_SRCS})

target_link_libraries(kwmtheme  ${KDE4_KDECORE_LIBS} )

install(TARGETS kwmtheme DESTINATION ${BIN_INSTALL_DIR})

