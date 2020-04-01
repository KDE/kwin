#######################################
# Config
set(kwin_slide_config_SRCS slide_config.cpp)
ki18n_wrap_ui(kwin_slide_config_SRCS slide_config.ui)
kconfig_add_kcfg_files(kwin_slide_config_SRCS slideconfig.kcfgc)

add_library(kwin_slide_config MODULE ${kwin_slide_config_SRCS})

target_link_libraries(kwin_slide_config
    KF5::ConfigWidgets
    KF5::I18n
    Qt5::DBus
    KWinEffectsInterface
)

kcoreaddons_desktop_to_json(kwin_slide_config slide_config.desktop SERVICE_TYPES kcmodule.desktop)

install(
    TARGETS
        kwin_slide_config
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kwin/effects/configs
)
