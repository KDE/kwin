#######################################
# Effect

# Source files
set(fallapart_SOURCES
    fallapart.cpp
    main.cpp
)

kconfig_add_kcfg_files(fallapart_SOURCES
    fallapartconfig.kcfgc
)

kwin4_add_effect_module(fallapart ${fallapart_SOURCES})
target_link_libraries(fallapart PRIVATE
    kwineffects

    KF6::ConfigGui
)
