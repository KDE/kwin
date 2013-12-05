configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/version.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(kcm_SRCS
    main.cpp
    module.cpp
)

kde4_add_ui_files(kcm_SRCS module.ui)

add_library(kcm_kwin_scripts MODULE ${kcm_SRCS})

target_link_libraries(kcm_kwin_scripts
    Qt5::DBus
    KF5::KCMUtils
    KF5::KIOCore
    KF5::KI18n
    KF5::Plasma
    KF5::KNewStuff
)

install(TARGETS kcm_kwin_scripts DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES kwinscripts.desktop DESTINATION ${SERVICES_INSTALL_DIR})
install(FILES kwinscripts.knsrc  DESTINATION  ${CONFIG_INSTALL_DIR})
