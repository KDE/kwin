#######################################
# Effect

set(slide_SOURCES
    main.cpp
    slide.cpp
    springmotion.cpp
)

kconfig_add_kcfg_files(slide_SOURCES
    slideconfig.kcfgc
)

kwin4_add_effect_module(kwin4_effect_slide ${slide_SOURCES})
target_link_libraries(kwin4_effect_slide PRIVATE
    kwineffects

    KF5::ConfigGui
)

#######################################
# Config
if (KWIN_BUILD_KCMS)
    set(kwin_slide_config_SRCS slide_config.cpp)
    ki18n_wrap_ui(kwin_slide_config_SRCS slide_config.ui)
    kconfig_add_kcfg_files(kwin_slide_config_SRCS slideconfig.kcfgc)

    kwin_add_effect_config(kwin_slide_config ${kwin_slide_config_SRCS})

    target_link_libraries(kwin_slide_config
        KF5::ConfigWidgets
        KF5::CoreAddons
        KF5::I18n
        Qt::DBus
        KWinEffectsInterface
    )
endif()
