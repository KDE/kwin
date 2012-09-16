#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    flipswitch/flipswitch.cpp
    )

kde4_add_kcfg_files(kwin4_effect_builtins_sources flipswitch/flipswitchconfig.kcfgc)

# .desktop files
install( FILES
    flipswitch/flipswitch.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

#######################################
# Config

# Source files
set( kwin4_effect_builtins_config_sources ${kwin4_effect_builtins_config_sources}
    flipswitch/flipswitch_config.cpp
    flipswitch/flipswitch_config.ui
    )

kde4_add_kcfg_files(kwin4_effect_builtins_config_sources flipswitch/flipswitchconfig.kcfgc)

# .desktop files
install( FILES
    flipswitch/flipswitch_config.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
