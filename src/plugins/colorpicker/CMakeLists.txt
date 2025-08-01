#######################################
# Effect

kwin_add_builtin_effect(colorpicker)
target_sources(colorpicker
    PRIVATE FILE_SET CXX_MODULES FILES
    colorpicker.cpp
    main.cpp
)
target_link_libraries(colorpicker PRIVATE
    kwin

    KF6::I18n

    Qt::DBus
)
