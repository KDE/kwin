#######################################
# Effect

#######################################
# Config
set(kwin_icc_config_SRCS icc_config.cpp)
ki18n_wrap_ui(kwin_icc_config_SRCS icc_config.ui)
qt5_add_dbus_interface(kwin_icc_config_SRCS ${kwin_effects_dbus_xml} kwineffects_interface)
kconfig_add_kcfg_files(kwin_icc_config_SRCS iccconfig.kcfgc)

add_library(kwin_icc_config MODULE ${kwin_icc_config_SRCS})

## Build separate plugin
#find_package(PkgConfig)
#pkg_check_modules(LCMS2 REQUIRED lcms2)
#set_package_properties(LCMS2 PROPERTIES TYPE REQUIRED PURPOSE "Required for ICC color correction.")
#
#kwin4_add_effect(icc icc.cpp)
#target_link_libraries(kwin4_effect_icc PUBLIC ${LCMS2_LIBRARIES})
#target_include_directories(kwin4_effect_icc PUBLIC ${LCMS2_INCLUDE_DIRS})
#target_compile_options(kwin4_effect_icc PUBLIC ${LCMS2_CFLAGS_OTHER})

target_link_libraries(kwin_icc_config
    KF5::ConfigWidgets
    KF5::I18n
    KF5::Service
    KF5::XmlGui
)

kcoreaddons_desktop_to_json(kwin_icc_config icc_config.desktop SERVICE_TYPES kcmodule.desktop)

install(
    TARGETS
        kwin_icc_config
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kwin/effects/configs
)
