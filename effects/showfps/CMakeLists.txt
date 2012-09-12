#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    showfps/showfps.cpp
    )

kde4_add_kcfg_files(kwin4_effect_builtins_sources showfps/showfpsconfig.kcfgc)

# .desktop files
install( FILES
    showfps/showfps.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

#######################################
# Config

# Source files
set( kwin4_effect_builtins_config_sources ${kwin4_effect_builtins_config_sources}
    showfps/showfps_config.cpp
    showfps/showfps_config.ui
    )

kde4_add_kcfg_files(kwin4_effect_builtins_config_sources showfps/showfpsconfig.kcfgc)

# .desktop files
install( FILES
    showfps/showfps_config.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
