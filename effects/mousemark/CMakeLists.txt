#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    mousemark/mousemark.cpp
    )

kde4_add_kcfg_files(kwin4_effect_builtins_sources mousemark/mousemarkconfig.kcfgc)

# .desktop files
install( FILES
    mousemark/mousemark.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

#######################################
# Config

# Source files
set( kwin4_effect_builtins_config_sources ${kwin4_effect_builtins_config_sources}
    mousemark/mousemark_config.cpp
    mousemark/mousemark_config.ui
    )

kde4_add_kcfg_files(kwin4_effect_builtins_config_sources mousemark/mousemarkconfig.kcfgc)

# .desktop files
install( FILES
    mousemark/mousemark_config.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
