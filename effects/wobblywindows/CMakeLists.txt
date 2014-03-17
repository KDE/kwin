#######################################
# Effect
install( FILES
    wobblywindows.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

#######################################
# Config
set(kwin_wobblywindows_config_SRCS wobblywindows_config.cpp)
qt5_wrap_ui(kwin_wobblywindows_config_SRCS wobblywindows_config.ui)
kconfig_add_kcfg_files(kwin_wobblywindows_config_SRCS wobblywindowsconfig.kcfgc)

add_library(kwin_wobblywindows_config MODULE ${kwin_wobblywindows_config_SRCS})

target_link_libraries(kwin_wobblywindows_config
    kwineffects
    KF5::ConfigWidgets
    KF5::I18n
    KF5::Service
)

kservice_desktop_to_json(kwin_wobblywindows_config wobblywindows_config.desktop)

install(
    TARGETS
        kwin_wobblywindows_config
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kwin/effects/configs
)
