#######################################
# Effect

# Source files
set(sheet_SOURCES
    main.cpp
    sheet.cpp
)
kconfig_add_kcfg_files(sheet_SOURCES sheetconfig.kcfgc)

kwin_add_builtin_effect(sheet ${sheet_SOURCES})
target_link_libraries(sheet PRIVATE
    kwin

    KF6::ConfigGui
)
