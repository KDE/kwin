kwin_add_builtin_effect(reflectcursor)

target_sources(reflectcursor PRIVATE
    main.cpp
    reflectcursor.cpp
)

kconfig_add_kcfg_files(reflectcursor
    reflectcursorconfig.kcfgc
)

target_link_libraries(reflectcursor PRIVATE
    kwin

    KF6::ConfigGui
)
