#######################################
# Effect

set(mouseclick_SOURCES
    main.cpp
    mouseclick.cpp
)

kconfig_add_kcfg_files(mouseclick_SOURCES
    mouseclickconfig.kcfgc
)

kwin_add_builtin_effect(mouseclick ${mouseclick_SOURCES})
target_link_libraries(mouseclick PRIVATE
    kwin

    KF6::ConfigGui
    KF6::GlobalAccel
    KF6::I18n
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
        KF6::KCMUtils
        KF6::CoreAddons
        KF6::GlobalAccel
        KF6::I18n
        KF6::XmlGui
        KWinEffectsInterface
    )
endif()
