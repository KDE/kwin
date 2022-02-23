#######################################
# Effect

set(glide_SOURCES
    glide.cpp
    main.cpp
)

kconfig_add_kcfg_files(glide_SOURCES
    glideconfig.kcfgc
)

kwin4_add_effect_module(kwin4_effect_glide ${glide_SOURCES})
target_link_libraries(kwin4_effect_glide PRIVATE
    kwineffects

    KF5::ConfigGui
)

#######################################
# Config
if (KWIN_BUILD_KCMS)
    set(kwin_glide_config_SRCS glide_config.cpp)
    ki18n_wrap_ui(kwin_glide_config_SRCS glide_config.ui)
    kconfig_add_kcfg_files(kwin_glide_config_SRCS glideconfig.kcfgc)

    kwin_add_effect_config(kwin_glide_config ${kwin_glide_config_SRCS})

    target_link_libraries(kwin_glide_config
        KF5::ConfigWidgets
        KF5::CoreAddons
        KF5::I18n
        Qt::DBus
        KWinEffectsInterface
    )
endif()
