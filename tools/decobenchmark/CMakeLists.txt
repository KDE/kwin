


########### next target ###############

set(decobenchmark_SRCS main.cpp preview.cpp )

kde4_automoc(${decobenchmark_SRCS})

kde4_add_executable(decobenchmark ${decobenchmark_SRCS})

target_link_libraries(decobenchmark  ${KDE4_KDECORE_LIBS} kdecorations )

