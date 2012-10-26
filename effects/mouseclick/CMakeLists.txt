##########################
## mouse click effect
##########################

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    mouseclick/mouseclick.cpp
    )
kde4_add_kcfg_files(kwin4_effect_builtins_sources mouseclick/mouseclickconfig.kcfgc)

# .desktop files
install( FILES 
         mouseclick/mouseclick.desktop
         DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

##########################
## configurtion dialog 
##########################

# Source files
set( kwin4_effect_builtins_config_sources ${kwin4_effect_builtins_config_sources}
    mouseclick/mouseclick_config.cpp
    mouseclick/mouseclick_config.ui
    )

kde4_add_kcfg_files(kwin4_effect_builtins_config_sources mouseclick/mouseclickconfig.kcfgc)

install( FILES 
         mouseclick/mouseclick_config.desktop
         DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
