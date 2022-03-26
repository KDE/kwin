#######################################
# Effect

# Source files
set(startupfeedback_SOURCES
    main.cpp
    startupfeedback.cpp
    startupfeedback.qrc
)

kwin4_add_effect_module(kwin4_effect_startupfeedback ${startupfeedback_SOURCES})
target_link_libraries(kwin4_effect_startupfeedback PRIVATE
    kwineffects
    kwinglutils

    Qt::DBus
    Qt::Widgets
)
