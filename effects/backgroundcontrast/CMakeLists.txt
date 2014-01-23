#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
     backgroundcontrast/contrast.cpp
     backgroundcontrast/contrastshader.cpp )

# .desktop files
install( FILES
         backgroundcontrast/contrast.desktop
         DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
