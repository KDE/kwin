#######################################
# Effect

# Source files
set(kscreen_SOURCES
    kscreen.cpp
    main.cpp
)

kconfig_add_kcfg_files(kscreen_SOURCES kscreenconfig.kcfgc)
kwin4_add_effect_module(kwin4_effect_kscreen ${kscreen_SOURCES})
target_link_libraries(kwin4_effect_kscreen PRIVATE
    kwineffects

    KF5::ConfigGui
)
