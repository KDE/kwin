#######################################
# Effect

set(blendchanges_SOURCES
    main.cpp
    blendchanges.cpp
)

kwin_add_builtin_effect(blendchanges ${blendchanges_SOURCES})
target_link_libraries(blendchanges PRIVATE
    kwin
    Qt::DBus
)
