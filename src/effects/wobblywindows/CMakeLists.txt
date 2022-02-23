#######################################
# Effect

set(wobblywindows_SOURCES
    main.cpp
    wobblywindows.cpp
)

kconfig_add_kcfg_files(wobblywindows_SOURCES
    wobblywindowsconfig.kcfgc
)

kwin4_add_effect_module(kwin4_effect_wobblywindows ${wobblywindows_SOURCES})
target_link_libraries(kwin4_effect_wobblywindows PRIVATE
    kwineffects

    KF5::ConfigGui
)

#######################################
# Config
if (KWIN_BUILD_KCMS)
    set(kwin_wobblywindows_config_SRCS wobblywindows_config.cpp)
    ki18n_wrap_ui(kwin_wobblywindows_config_SRCS wobblywindows_config.ui)
    kconfig_add_kcfg_files(kwin_wobblywindows_config_SRCS wobblywindowsconfig.kcfgc)

    kwin_add_effect_config(kwin_wobblywindows_config ${kwin_wobblywindows_config_SRCS})

    target_link_libraries(kwin_wobblywindows_config
        KF5::ConfigWidgets
        KF5::CoreAddons
        KF5::I18n
        Qt::DBus
        KWinEffectsInterface
    )
endif()
