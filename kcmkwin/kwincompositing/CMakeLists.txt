
cmake_minimum_required(VERSION 2.8.10.1)

project(kwin-compositing-kcm)

################# Disallow in-source build #################

# Make CPack available to easy generate binary packages
include(CPack)

include(FeatureSummary)

################# set KDE specific information #################

find_package(ECM 0.0.8 REQUIRED NO_MODULE)

# where to look first for cmake modules, before ${CMAKE_ROOT}/Modules/ is checked

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})


find_package(KF5CoreAddons REQUIRED NO_MODULE)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings)

find_package(Qt5Core REQUIRED NO_MODULE)
find_package(Qt5 REQUIRED NO_MODULE COMPONENTS DBus Quick Test Widgets)

# check for Qt5Multimedia (runtime-only dependency)
find_package(Qt5Multimedia QUIET)
set_package_properties(Qt5Multimedia PROPERTIES
         PURPOSE "Runtime-only dependency for effect video playback"
         TYPE RUNTIME
         )

add_definitions(-DQT_DISABLE_DEPRECATED_BEFORE=0 -std=c++0x)

if(KDE_PLATFORM_FEATURE_DISABLE_DEPRECATED)
   set(KDE_NO_DEPRECATED TRUE)
   set(CMAKE_AUTOMOC_MOC_OPTIONS "-DKDE_NO_DEPRECATED")
endif()

################# now find all used packages #################

set (QT_MIN_VERSION "5.2.0")


# Load CMake, Compiler and InstallDirs settings from KF5 and the following are already somewhat "done" tier1/tier2 libs from kdelibs:
find_package(KF5 CONFIG REQUIRED COMPONENTS CoreAddons Config I18n Service XmlGui KCMUtils NewStuff)


find_package(KF5Declarative REQUIRED NO_MODULE)

# Required for KWin's DBus interfaces
find_package(KDE4Workspace REQUIRED NO_MODULE)
set_package_properties(KDE4Workspace PROPERTIES
                       TYPE REQUIRED
                       PURPOSE "Required for DBus interfaces to KWin"
                      )

#########################################################################

add_definitions(-DQT_USE_FAST_CONCATENATION -DQT_USE_FAST_OPERATOR_PLUS)
add_definitions(-DQT_NO_URL_CAST_FROM_STRING)

remove_definitions(-DQT_NO_CAST_FROM_ASCII -DQT_STRICT_ITERATORS -DQT_NO_CAST_FROM_BYTEARRAY)

################# configure checks and create the configured files #################

# now create config headers
configure_file(config-prefix.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-prefix.h )
configure_file(config-compiler.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-compiler.h )

set(CMAKE_CXX_FLAGS "${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS}")

set(kwincomposing_SRC
    model.cpp
    main.cpp
    compositing.cpp
    effectconfig.cpp)

qt5_add_dbus_interface( kwincomposing_SRC
  ${KDE4WORKSPACE_DBUS_INTERFACES_DIR}/org.kde.kwin.Compositing.xml kwin_compositing_interface)
qt5_add_dbus_interface( kwincomposing_SRC
  ${KDE4WORKSPACE_DBUS_INTERFACES_DIR}/org.kde.kwin.Effects.xml kwin_effects_interface)

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
  ${KDE4WORKSPACE_DBUS_INTERFACES_DIR}/org.kde.kwin.Compositing.xml kwin_compositing_interface)
qt5_add_dbus_interface( modelTest_SRC
  ${KDE4WORKSPACE_DBUS_INTERFACES_DIR}/org.kde.kwin.Effects.xml kwin_effects_interface)

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

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
