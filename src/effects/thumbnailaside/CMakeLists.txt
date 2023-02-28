#######################################
# Effect

set(thumbnailaside_SOURCES
    main.cpp
    thumbnailaside.cpp
)

kconfig_add_kcfg_files(thumbnailaside_SOURCES
    thumbnailasideconfig.kcfgc
)

kwin4_add_effect_module(thumbnailaside ${thumbnailaside_SOURCES})
target_link_libraries(thumbnailaside PRIVATE
    kwineffects
    kwinglutils

    KF6::ConfigGui
    KF6::GlobalAccel
    KF6::I18n
)

#######################################
# Config
if (KWIN_BUILD_KCMS)
    set(kwin_thumbnailaside_config_SRCS thumbnailaside_config.cpp)
    ki18n_wrap_ui(kwin_thumbnailaside_config_SRCS thumbnailaside_config.ui)
    kconfig_add_kcfg_files(kwin_thumbnailaside_config_SRCS thumbnailasideconfig.kcfgc)

    kwin_add_effect_config(kwin_thumbnailaside_config ${kwin_thumbnailaside_config_SRCS})

    target_link_libraries(kwin_thumbnailaside_config
        KF6::ConfigWidgets
        KF6::CoreAddons
        KF6::GlobalAccel
        KF6::I18n
        KF6::XmlGui
        KWinEffectsInterface
    )
endif()
