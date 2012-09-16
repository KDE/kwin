#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
     blur/blur.cpp
     blur/blurshader.cpp )

kde4_add_kcfg_files(kwin4_effect_builtins_sources blur/blurconfig.kcfgc)

# .desktop files
install( FILES
         blur/blur.desktop
         DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

#######################################
# Config

# Source files
set( kwin4_effect_builtins_config_sources ${kwin4_effect_builtins_config_sources}
     blur/blur_config.cpp
     blur/blur_config.ui )

kde4_add_kcfg_files(kwin4_effect_builtins_config_sources blur/blurconfig.kcfgc)

# .desktop files
install( FILES
         blur/blur_config.desktop
         DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

