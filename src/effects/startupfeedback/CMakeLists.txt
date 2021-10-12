#######################################
# Effect

# Source files
set(startupfeedback_SOURCES
    main.cpp
    startupfeedback.cpp
)

kwin4_add_effect_module(kwin4_effect_startupfeedback ${startupfeedback_SOURCES})
