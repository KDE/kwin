########### next target ###############

set(kwin_opengl_test_SRCS opengltest.cpp )

kde4_add_executable(kwin_opengl_test ${kwin_opengl_test_SRCS})

target_link_libraries(kwin_opengl_test ${X11_LIBRARIES} ${OPENGL_gl_LIBRARY})

install(TARGETS kwin_opengl_test ${INSTALL_TARGETS_DEFAULT_ARGS} )

