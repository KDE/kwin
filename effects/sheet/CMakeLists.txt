#######################################
# Effect

# Source files
set( kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    sheet/sheet.cpp
    )
kde4_add_kcfg_files(kwin4_effect_builtins_sources sheet/sheetconfig.kcfgc)

# .desktop files
install( FILES
    sheet/sheet.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

