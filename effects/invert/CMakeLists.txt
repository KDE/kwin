#######################################
# Effect

#######################################
# Config
set(kwin_invert_config_SRCS invert_config.cpp)

add_library(kwin_invert_config MODULE ${kwin_invert_config_SRCS})

target_link_libraries(kwin_invert_config
    KF5::ConfigWidgets
    KF5::GlobalAccel
    KF5::I18n
    KF5::XmlGui
    KWinEffectsInterface
)

kcoreaddons_desktop_to_json(kwin_invert_config invert_config.desktop SERVICE_TYPES kcmodule.desktop)

install(
    TARGETS
        kwin_invert_config
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kwin/effects/configs
)
