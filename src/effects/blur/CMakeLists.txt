#######################################
# Effect

set(blur_SOURCES
    blur.cpp
    blur.qrc
    blurshader.cpp
    main.cpp
)

kconfig_add_kcfg_files(blur_SOURCES
    blurconfig.kcfgc
)

kwin4_add_effect_module(kwin4_effect_blur ${blur_SOURCES})
target_link_libraries(kwin4_effect_blur PRIVATE
    kwin
    kwineffects
    kwinglutils

    KF5::ConfigGui

    KDecoration2::KDecoration
)

#######################################
# Config
if (KWIN_BUILD_KCMS)
    set(kwin_blur_config_SRCS blur_config.cpp)
    ki18n_wrap_ui(kwin_blur_config_SRCS blur_config.ui)
    kconfig_add_kcfg_files(kwin_blur_config_SRCS blurconfig.kcfgc)

    kwin_add_effect_config(kwin_blur_config ${kwin_blur_config_SRCS})

    target_link_libraries(kwin_blur_config
        KF5::ConfigWidgets
        KF5::CoreAddons
        KF5::I18n
        Qt::DBus
        KWinEffectsInterface
    )
endif()
