# Adds effect plugin with given name. Sources are given after the name
macro(KWIN4_ADD_EFFECT name)
    kde4_automoc(kwin4_effect_${name} ${ARGN})
    kde4_add_plugin(kwin4_effect_${name} ${ARGN})
    target_link_libraries(kwin4_effect_${name} kwineffects ${KDE4_KDEUI_LIBS})
    install(TARGETS kwin4_effect_${name} DESTINATION ${PLUGIN_INSTALL_DIR})
endmacro(KWIN4_ADD_EFFECT)


include_directories(
    ${CMAKE_SOURCE_DIR}/workspace/kwin/lib
    )

KWIN4_ADD_EFFECT(builtins presentwindows.cpp shadow.cpp diminactive.cpp)

install( FILES presentwindows.desktop shadow.desktop diminactive.desktop
        DESTINATION ${DATA_INSTALL_DIR}/kwin/effects )
