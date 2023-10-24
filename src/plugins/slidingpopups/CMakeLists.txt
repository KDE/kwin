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

kwin_add_builtin_effect(slidingpopups ${slidingpopups_SOURCES})
target_link_libraries(slidingpopups PRIVATE
    kwin

    KF6::ConfigGui
)
