#########################################################################

add_definitions(-DQT_USE_FAST_CONCATENATION -DQT_USE_FAST_OPERATOR_PLUS)
add_definitions(-DQT_NO_URL_CAST_FROM_STRING)

remove_definitions(-DQT_NO_CAST_FROM_ASCII -DQT_STRICT_ITERATORS -DQT_NO_CAST_FROM_BYTEARRAY)

################# configure checks and create the configured files #################

# now create config headers
configure_file(config-prefix.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-prefix.h )
configure_file(config-compiler.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-compiler.h )

set(kwincomposing_SRC
    model.cpp
    main.cpp
    compositing.cpp
    effectconfig.cpp)

qt5_add_dbus_interface( kwincomposing_SRC
  ${KWIN_SOURCE_DIR}/org.kde.kwin.Compositing.xml kwin_compositing_interface)
qt5_add_dbus_interface( kwincomposing_SRC
  ${KWIN_SOURCE_DIR}/org.kde.kwin.Effects.xml kwin_effects_interface)

add_library(kwincompositing MODULE ${kwincomposing_SRC})

target_link_libraries(kwincompositing
    Qt5::Quick
    Qt5::DBus
    Qt5::Widgets
    KF5::CoreAddons
    KF5::ConfigCore
    KF5::Declarative
    KF5::I18n
    KF5::Service
    KF5::KCMUtils
    KF5::NewStuff
    KF5::Declarative
)


set(modelTest_SRC
    model.cpp
    effectconfig.cpp
    compositing.cpp
    test/effectmodeltest.cpp
    test/modeltest.cpp)

qt5_add_dbus_interface( modelTest_SRC
  ${KWIN_SOURCE_DIR}/org.kde.kwin.Compositing.xml kwin_compositing_interface)
qt5_add_dbus_interface( modelTest_SRC
  ${KWIN_SOURCE_DIR}/org.kde.kwin.Effects.xml kwin_effects_interface)

add_executable(effectModelTest ${modelTest_SRC})

target_link_libraries(effectModelTest
    Qt5::Quick
    Qt5::DBus
    Qt5::Test
    Qt5::Widgets
    KF5::CoreAddons
    KF5::ConfigCore
    KF5::Declarative
    KF5::I18n
    KF5::Service
    KF5::KCMUtils
    KF5::NewStuff
    KF5::Declarative)

INSTALL(DIRECTORY qml DESTINATION ${DATA_INSTALL_DIR}/kwincompositing)
INSTALL(TARGETS kwincompositing DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES kwincompositing.desktop kcmkwineffects.desktop  DESTINATION  ${SERVICES_INSTALL_DIR})
install(FILES kwineffect.knsrc DESTINATION ${CONFIG_INSTALL_DIR})
################# list the subdirectories #################
