#######################################
# Effect

set(mouseclick_SOURCES
    main.cpp
    mouseclick.cpp
)

kconfig_add_kcfg_files(mouseclick_SOURCES
    mouseclickconfig.kcfgc
)

kwin4_add_effect_module(kwin4_effect_mouseclick ${mouseclick_SOURCES})
target_link_libraries(kwin4_effect_mouseclick PRIVATE
    kwineffects
    kwinglutils

    KF5::ConfigGui
    KF5::GlobalAccel
    KF5::I18n
)

##########################
## configurtion dialog
##########################
if (KWIN_BUILD_KCMS)
    set(kwin_mouseclick_config_SRCS mouseclick_config.cpp)
    ki18n_wrap_ui(kwin_mouseclick_config_SRCS mouseclick_config.ui)
    kconfig_add_kcfg_files(kwin_mouseclick_config_SRCS mouseclickconfig.kcfgc)

    kwin_add_effect_config(kwin_mouseclick_config ${kwin_mouseclick_config_SRCS})

    target_link_libraries(kwin_mouseclick_config
        KF5::ConfigWidgets
        KF5::CoreAddons
        KF5::GlobalAccel
        KF5::I18n
        KF5::XmlGui
        KWinEffectsInterface
    )
endif()
