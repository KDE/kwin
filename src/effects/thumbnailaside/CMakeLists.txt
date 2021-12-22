#######################################
# Effect

set(thumbnailaside_SOURCES
    main.cpp
    thumbnailaside.cpp
)

kconfig_add_kcfg_files(thumbnailaside_SOURCES
    thumbnailasideconfig.kcfgc
)

kwin4_add_effect_module(kwin4_effect_thumbnailaside ${thumbnailaside_SOURCES})

#######################################
# Config
set(kwin_thumbnailaside_config_SRCS thumbnailaside_config.cpp)
ki18n_wrap_ui(kwin_thumbnailaside_config_SRCS thumbnailaside_config.ui)
kconfig_add_kcfg_files(kwin_thumbnailaside_config_SRCS thumbnailasideconfig.kcfgc)

kwin_add_effect_config(kwin_thumbnailaside_config ${kwin_thumbnailaside_config_SRCS})

target_link_libraries(kwin_thumbnailaside_config
    KF5::ConfigWidgets
    KF5::GlobalAccel
    KF5::I18n
    KF5::XmlGui
    KWinEffectsInterface
)
