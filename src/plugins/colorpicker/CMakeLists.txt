#######################################
# Effect

set(colorpicker_SOURCES
    colorpicker.cpp
    main.cpp
)

kwin_add_builtin_effect(colorpicker ${colorpicker_SOURCES})
target_link_libraries(colorpicker PRIVATE
    kwin

    KF6::I18n

    Qt::DBus
)
