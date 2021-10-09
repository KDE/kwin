#######################################
# Effect

# Source files
set(sheet_SOURCES
    sheet.cpp
)
kconfig_add_kcfg_files(sheet_SOURCES sheetconfig.kcfgc)

kwin4_add_effect_module(kwin4_effect_sheet ${sheet_SOURCES})
