#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    kicker/kicker.cpp
    )

# .desktop files
install( FILES
    kicker/kicker.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

#######################################
# Config

# Source files
#set( kwin4_effect_builtins_config_sources ${kwin4_effect_builtins_config_sources}
    #cube/cube_config.cpp
    #cube/cube_config.ui
    #cube/cubeslide_config.cpp
    #cube/cubeslide_config.ui
    #)

# .desktop files
#install( FILES
 #   cube/cube_config.desktop
  #  cube/cubeslide_config.desktop
   # DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
