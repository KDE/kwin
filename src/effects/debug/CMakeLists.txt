#######################################
# Effect

kwin4_add_effect_module(kwin4_effect_debug
    debugeffect.cpp
    debugeffect.qrc
    main.cpp
)

target_link_libraries(kwin4_effect_debug PRIVATE
    kwineffects
    kwinglutils

    KF5::GlobalAccel
    KF5::I18n
)

#######################################
# Config
if (KWIN_BUILD_KCMS)
    kwin_add_effect_config(kwin_debug_config debugeffectconfig.cpp)

    target_link_libraries(kwin_debug_config
        KF5::ConfigWidgets
        KF5::CoreAddons
        KF5::GlobalAccel
        KF5::I18n
        KF5::XmlGui
        KWinEffectsInterface
    )
endif()
