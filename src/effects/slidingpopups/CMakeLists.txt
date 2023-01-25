#######################################
# Effect

# Source files
set(slidingpopups_SOURCES
    main.cpp main.h
    slidingpopups.cpp slidingpopups.h
)

kconfig_add_kcfg_files(slidingpopups_SOURCES
    slidingpopupsconfig.kcfgc
)

kwin4_add_effect_module(kwin4_effect_slidingpopups ${slidingpopups_SOURCES})
target_link_libraries(kwin4_effect_slidingpopups PRIVATE
    kwin
    kwineffects

    KF5::ConfigGui
)
