#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    fade/fade.cpp
    )

# .desktop files
install( FILES
    fade/fade.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
