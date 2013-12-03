#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
     backgroundcontrast/contrast.cpp
     backgroundcontrast/contrastshader.cpp )

kde4_add_kcfg_files(kwin4_effect_builtins_sources backgroundcontrast/contrastconfig.kcfgc)

# .desktop files
install( FILES
         backgroundcontrast/contrast.desktop
         DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
