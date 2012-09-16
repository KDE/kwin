#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    glide/glide.cpp
    )

kde4_add_kcfg_files(kwin4_effect_builtins_sources glide/glideconfig.kcfgc)

# .desktop files
install( FILES
    glide/glide.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

#######################################
# Config

# Source files
set( kwin4_effect_builtins_config_sources ${kwin4_effect_builtins_config_sources}
     glide/glide_config.cpp
     glide/glide_config.ui )

kde4_add_kcfg_files(kwin4_effect_builtins_config_sources glide/glideconfig.kcfgc)

# .desktop files
install( FILES
         glide/glide_config.desktop
         DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

