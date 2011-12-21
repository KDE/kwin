configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/version.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(kcm_SRCS
    main.cpp
    module.cpp
)

kde4_add_ui_files(kcm_SRCS module.ui)

kde4_add_plugin(kcm_kwin_scripts ${kcm_SRCS})

target_link_libraries(kcm_kwin_scripts
    ${KDE4_KDEUI_LIBRARY}
    ${KDE4_KIO_LIBS}
)

install(TARGETS kcm_kwin_scripts DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES kwinscripts.desktop DESTINATION ${SERVICES_INSTALL_DIR})
