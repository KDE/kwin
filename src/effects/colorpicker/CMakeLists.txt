#######################################
# Effect

set(colorpicker_SOURCES
    colorpicker.cpp
    main.cpp
)

kwin4_add_effect_module(colorpicker ${colorpicker_SOURCES})
target_link_libraries(colorpicker PRIVATE
    kwineffects

    KF6::I18n

    Qt::DBus
)
