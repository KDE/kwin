#######################################
# Config
set(kwin_blur_config_SRCS blur_config.cpp)
ki18n_wrap_ui(kwin_blur_config_SRCS blur_config.ui)
kconfig_add_kcfg_files(kwin_blur_config_SRCS blurconfig.kcfgc)

add_library(kwin_blur_config MODULE ${kwin_blur_config_SRCS})

target_link_libraries(kwin_blur_config
    KF5::ConfigWidgets
    KF5::I18n
    Qt5::DBus
    KWinEffectsInterface
)

kcoreaddons_desktop_to_json(kwin_blur_config blur_config.desktop SERVICE_TYPES kcmodule.desktop)

install(
    TARGETS
        kwin_blur_config
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kwin/effects/configs
)
