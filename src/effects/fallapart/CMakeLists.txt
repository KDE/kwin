#######################################
# Effect

# Source files
set(fallapart_SOURCES
    fallapart.cpp
)

kconfig_add_kcfg_files(fallapart_SOURCES
    fallapartconfig.kcfgc
)

kwin4_add_effect_module(kwin4_effect_fallapart ${fallapart_SOURCES})
