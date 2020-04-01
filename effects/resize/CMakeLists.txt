#######################################
# Config
set(kwin_resize_config_SRCS resize_config.cpp)
ki18n_wrap_ui(kwin_resize_config_SRCS resize_config.ui)
kconfig_add_kcfg_files(kwin_resize_config_SRCS resizeconfig.kcfgc)

add_library(kwin_resize_config MODULE ${kwin_resize_config_SRCS})

target_link_libraries(kwin_resize_config
    KF5::ConfigWidgets
    KF5::I18n
    Qt5::DBus
    KWinEffectsInterface
)

kcoreaddons_desktop_to_json(kwin_resize_config resize_config.desktop SERVICE_TYPES kcmodule.desktop)

install(
    TARGETS
        kwin_resize_config
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kwin/effects/configs
)
