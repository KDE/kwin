#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    slideback/slideback.cpp
    )

# .desktop files
install( FILES
    slideback/slideback.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
