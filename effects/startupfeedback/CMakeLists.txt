#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    startupfeedback/startupfeedback.cpp
    )

# .desktop files
install( FILES
    startupfeedback/startupfeedback.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

#######################################
# Config
