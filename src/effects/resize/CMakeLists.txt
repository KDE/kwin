#######################################
# Effect

set(resize_SOURCES
    main.cpp
    resize.cpp
)

kconfig_add_kcfg_files(resize_SOURCES
    resizeconfig.kcfgc
)

kwin4_add_effect_module(kwin4_effect_resize ${resize_SOURCES})

#######################################
# Config
set(kwin_resize_config_SRCS resize_config.cpp)
ki18n_wrap_ui(kwin_resize_config_SRCS resize_config.ui)
kconfig_add_kcfg_files(kwin_resize_config_SRCS resizeconfig.kcfgc)

kwin_add_effect_config(kwin_resize_config ${kwin_resize_config_SRCS})

target_link_libraries(kwin_resize_config
    KF5::ConfigWidgets
    KF5::I18n
    Qt::DBus
    KWinEffectsInterface
)
