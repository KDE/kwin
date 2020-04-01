#######################################
# Config
set(kwin_wobblywindows_config_SRCS wobblywindows_config.cpp)
ki18n_wrap_ui(kwin_wobblywindows_config_SRCS wobblywindows_config.ui)
kconfig_add_kcfg_files(kwin_wobblywindows_config_SRCS wobblywindowsconfig.kcfgc)

add_library(kwin_wobblywindows_config MODULE ${kwin_wobblywindows_config_SRCS})

target_link_libraries(kwin_wobblywindows_config
    KF5::ConfigWidgets
    KF5::I18n
    Qt5::DBus
    KWinEffectsInterface
)

kcoreaddons_desktop_to_json(kwin_wobblywindows_config wobblywindows_config.desktop SERVICE_TYPES kcmodule.desktop)

install(
    TARGETS
        kwin_wobblywindows_config
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kwin/effects/configs
)
