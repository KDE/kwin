#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    magnifier/magnifier.cpp
    )

kde4_add_kcfg_files(kwin4_effect_builtins_sources magnifier/magnifierconfig.kcfgc)

# .desktop files
install( FILES
    magnifier/magnifier.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

#######################################
# Config

# Source files
set( kwin4_effect_builtins_config_sources ${kwin4_effect_builtins_config_sources}
    magnifier/magnifier_config.cpp
    magnifier/magnifier_config.ui
    )
kde4_add_kcfg_files(kwin4_effect_builtins_config_sources magnifier/magnifierconfig.kcfgc)

# .desktop files
install( FILES
    magnifier/magnifier_config.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
