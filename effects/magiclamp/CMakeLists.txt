#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    magiclamp/magiclamp.cpp
    )

kde4_add_kcfg_files(kwin4_effect_builtins_sources magiclamp/magiclampconfig.kcfgc)

# .desktop files
install( FILES
    magiclamp/magiclamp.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

#######################################
# Config

# Source files
set( kwin4_effect_builtins_config_sources ${kwin4_effect_builtins_config_sources}
    magiclamp/magiclamp_config.cpp
    magiclamp/magiclamp_config.ui
    )

kde4_add_kcfg_files(kwin4_effect_builtins_config_sources magiclamp/magiclampconfig.kcfgc)

# .desktop files
install( FILES
    magiclamp/magiclamp_config.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
