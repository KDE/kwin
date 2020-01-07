#########################################################################
# KI18N Translation Domain for this library
add_definitions(-DTRANSLATION_DOMAIN=\"kcmkwincompositing\")

add_definitions(-DQT_NO_URL_CAST_FROM_STRING)

remove_definitions(-DQT_NO_CAST_FROM_ASCII -DQT_STRICT_ITERATORS -DQT_NO_CAST_FROM_BYTEARRAY)

################# configure checks and create the configured files #################

set(kwincomposing_SRC
    main.cpp
    compositing.cpp
)

qt5_add_dbus_interface(kwincomposing_SRC
    ${KWin_SOURCE_DIR}/org.kde.kwin.Compositing.xml kwin_compositing_interface
)

ki18n_wrap_ui(kwincomposing_SRC compositing.ui)

add_library(kwincompositing MODULE ${kwincomposing_SRC})

target_link_libraries(kwincompositing
    Qt5::DBus
    Qt5::Widgets

    KF5::ConfigCore
    KF5::CoreAddons
    KF5::I18n
    KF5::KCMUtils
)

install(TARGETS kwincompositing DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES kwincompositing.desktop DESTINATION ${SERVICES_INSTALL_DIR})
