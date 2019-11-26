#######################################

# Config
set(kwin_showpaint_config_SRCS showpaint_config.cpp)
ki18n_wrap_ui(kwin_showpaint_config_SRCS showpaint_config.ui)

add_library(kwin_showpaint_config MODULE ${kwin_showpaint_config_SRCS})

target_link_libraries(kwin_showpaint_config
    KF5::ConfigWidgets
    KF5::GlobalAccel
    KF5::I18n
    KF5::XmlGui
)

kcoreaddons_desktop_to_json(kwin_showpaint_config showpaint_config.desktop SERVICE_TYPES kcmodule.desktop)

install(
    TARGETS
        kwin_showpaint_config

    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kwin/effects/configs
)
