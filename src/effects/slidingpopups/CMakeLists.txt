#######################################
# Effect

# Source files
set(slidingpopups_SOURCES
    main.cpp
    slidingpopups.cpp
)

kconfig_add_kcfg_files(slidingpopups_SOURCES
    slidingpopupsconfig.kcfgc
)

kwin4_add_effect_module(slidingpopups ${slidingpopups_SOURCES})
target_link_libraries(slidingpopups PRIVATE
    kwin
    kwineffects

    KF6::ConfigGui
)
