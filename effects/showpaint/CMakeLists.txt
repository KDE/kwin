#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    showpaint/showpaint.cpp
    )

# .desktop files
install( FILES
    showpaint/showpaint.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
