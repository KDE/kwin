#######################################
# Config
set(kwin_thumbnailaside_config_SRCS thumbnailaside_config.cpp)
ki18n_wrap_ui(kwin_thumbnailaside_config_SRCS thumbnailaside_config.ui)
kconfig_add_kcfg_files(kwin_thumbnailaside_config_SRCS thumbnailasideconfig.kcfgc)

add_library(kwin_thumbnailaside_config MODULE ${kwin_thumbnailaside_config_SRCS})

target_link_libraries(kwin_thumbnailaside_config
    KF5::ConfigWidgets
    KF5::GlobalAccel
    KF5::I18n
    KF5::XmlGui
    KWinEffectsInterface
)

kcoreaddons_desktop_to_json(kwin_thumbnailaside_config thumbnailaside_config.desktop SERVICE_TYPES kcmodule.desktop)

install(
    TARGETS
        kwin_thumbnailaside_config
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kwin/effects/configs
)
