#######################################
# Effect

set(invert_SOURCES
    invert.cpp
    invert.qrc
    main.cpp
)

kwin4_add_effect_module(kwin4_effect_invert ${invert_SOURCES})
target_link_libraries(kwin4_effect_invert PRIVATE
    kwineffects
    kwinglutils

    KF5::GlobalAccel
    KF5::I18n
)

#######################################
# Config
if (KWIN_BUILD_KCMS)
    set(kwin_invert_config_SRCS invert_config.cpp)

    kwin_add_effect_config(kwin_invert_config ${kwin_invert_config_SRCS})

    target_link_libraries(kwin_invert_config
        KF5::ConfigWidgets
        KF5::CoreAddons
        KF5::GlobalAccel
        KF5::I18n
        KF5::XmlGui
        KWinEffectsInterface
    )
endif()
