#######################################
# Effect

# Source files
set(startupfeedback_SOURCES
    main.cpp
    startupfeedback.cpp
)

kwin_add_builtin_effect(startupfeedback ${startupfeedback_SOURCES})
target_link_libraries(startupfeedback PRIVATE
    kwin

    Qt::DBus
    Qt::Widgets
)
