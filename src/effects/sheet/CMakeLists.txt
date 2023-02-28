#######################################
# Effect

# Source files
set(sheet_SOURCES
    main.cpp
    sheet.cpp
)
kconfig_add_kcfg_files(sheet_SOURCES sheetconfig.kcfgc)

kwin4_add_effect_module(sheet ${sheet_SOURCES})
target_link_libraries(sheet PRIVATE
    kwineffects

    KF6::ConfigGui
)
