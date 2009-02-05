#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    mousemark/mousemark.cpp
    )

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

# .desktop files
install( FILES
    mousemark/mousemark_config.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
