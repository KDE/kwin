kwin_add_builtin_effect(shakecursor)

target_sources(shakecursor PRIVATE
    main.cpp
    shakecursor.cpp
    shakedetector.cpp
)

kconfig_add_kcfg_files(shakecursor
    shakecursorconfig.kcfgc
)

target_link_libraries(shakecursor PRIVATE
    kwin

    KF6::ConfigGui
)
