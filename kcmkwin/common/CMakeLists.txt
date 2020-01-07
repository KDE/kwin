# KI18N Translation Domain for this library
add_definitions(-DTRANSLATION_DOMAIN=\"kcmkwincommon\")

include_directories(${KWin_SOURCE_DIR}/effects)

set(kcmkwincommon_SRC
    effectsmodel.cpp
)

qt5_add_dbus_interface(kcmkwincommon_SRC
    ${KWin_SOURCE_DIR}/org.kde.kwin.Effects.xml kwin_effects_interface
)

add_library(kcmkwincommon SHARED ${kcmkwincommon_SRC})

target_link_libraries(kcmkwincommon
    Qt5::Core
    Qt5::DBus
    KF5::CoreAddons
    KF5::ConfigCore
    KF5::I18n
    KF5::Package
    KF5::KCMUtils
    kwin4_effect_builtins
)

set_target_properties(kcmkwincommon PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

install(TARGETS kcmkwincommon ${INSTALL_TARGETS_DEFAULT_ARGS} LIBRARY NAMELINK_SKIP)
