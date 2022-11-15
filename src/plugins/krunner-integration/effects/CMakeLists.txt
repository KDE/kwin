add_definitions(
    -DTRANSLATION_DOMAIN=\"effectsrunner\"
)

set(effectsrunner_SOURCES
    effectsrunner.cpp
    effectsrunner.h
)

kcoreaddons_add_plugin(effectsrunner
    SOURCES ${effectsrunner_SOURCES} 
    INSTALL_NAMESPACE "kf${QT_MAJOR_VERSION}/krunner"
)

target_link_libraries(effectsrunner
    KF6::Runner
    KF6::I18n
    Qt::DBus
)
