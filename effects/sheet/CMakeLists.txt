#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    sheet/sheet.cpp
    )

# .desktop files
install( FILES
    sheet/sheet.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

