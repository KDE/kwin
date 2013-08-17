
cmake_minimum_required(VERSION 2.8.10.1)

project(kwineffects)

################# Disallow in-source build #################

# Make CPack available to easy generate binary packages
include(CPack)

include(FeatureSummary)

################# set KDE specific information #################

find_package(ECM 0.0.8 REQUIRED NO_MODULE)

# where to look first for cmake modules, before ${CMAKE_ROOT}/Modules/ is checked

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH})

find_package(Qt5Transitional MODULE)
find_package(Qt5Core REQUIRED NO_MODULE)

find_package(Qt5Quick REQUIRED NO_MODULE)
find_package(KCoreAddons REQUIRED NO_MODULE)
find_package(Qt5Test REQUIRED NO_MODULE)

add_definitions(-DQT_DISABLE_DEPRECATED_BEFORE=0 -std=c++0x)

if(KDE_PLATFORM_FEATURE_DISABLE_DEPRECATED)
   set(KDE_NO_DEPRECATED TRUE)
   set(CMAKE_AUTOMOC_MOC_OPTIONS "-DKDE_NO_DEPRECATED")
endif()

################# now find all used packages #################

set (QT_MIN_VERSION "5.2.0")

find_package(kdeqt5staging REQUIRED NO_MODULE)

# Load CMake, Compiler and InstallDirs settings from KF5 and the following are already somewhat "done" tier1/tier2 libs from kdelibs:
find_package(KF5 MODULE REQUIRED COMPONENTS CMake Compiler InstallDirs
                                            KIdleTime ItemModels KWidgetsAddons KWindowSystem KCodecs KArchive KCoreAddons Solid ThreadWeaver
                                            KConfig KAuth KJS KWallet KDBusAddons
                                            KI18n KGuiAddons KService KWidgets ItemViews KNotifications KIconThemes KCompletion KJobWidgets KConfigWidgets Sonnet KTextWidgets XmlGui KCrash
                                            KIO KUnitConversion KDE4Attic)
find_package(KDELibs4 REQUIRED NO_MODULE)
find_package(KDE4Support REQUIRED NO_MODULE)

find_package(kdeclarative REQUIRED NO_MODULE)
find_package(LibAttica REQUIRED NO_MODULE)

#########################################################################

#add_definitions(${KDE4_DEFINITIONS})
add_definitions(-DQT_USE_FAST_CONCATENATION -DQT_USE_FAST_OPERATOR_PLUS)
add_definitions(-DQT_NO_URL_CAST_FROM_STRING)

remove_definitions(-DQT_NO_CAST_FROM_ASCII -DQT_STRICT_ITERATORS -DQT_NO_CAST_FROM_BYTEARRAY -DQT_NO_KEYWORDS)

################# setup the include directories #################
include_directories(${KDE4_INCLUDES}
                    ${kdeqt5staging_INCLUDE_DIR}
                    ${kdeqt5staging_INCLUDE_DIRS}
                    ${KF5_INCLUDE_DIRS}  # since e-c-m 0.0.7
                    ${Qt5Widgets_INCLUDE_DIRS}
                   )

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


add_executable(kwincompositing ${kwincomposing_SRC})

target_link_libraries(kwincompositing
    ${Qt5Quick_LIBRARIES}
    ${Qt5Qml_LIBRARIES}
    ${Qt5Core_LIBARIES}
    ${kservice_LIBRARIES}
    ${Qt5Widgets_LIBRARIES}
    ${KDE4_KDECORE_LIBS}
    ${KDE4_KCMUTILS_LIBS}
    ${Qt5Test_LIBRARIES}
    ${KDE4Support_LIBRARIES}
    ${KCoreAddons_LIBRARIES})

set(modelTest_SRC
    model.cpp
    effectconfig.cpp
    compositing.cpp
    test/effectmodeltest.cpp
    test/modeltest.cpp)

add_executable(effectModelTest ${modelTest_SRC})

target_link_libraries(effectModelTest
    ${Qt5Quick_LIBRARIES}
    ${Qt5Qml_LIBRARIES}
    ${Qt5Core_LIBARIES}
    ${kservice_LIBRARIES}
    ${Qt5Widgets_LIBRARIES}
    ${KDE4_KDECORE_LIBS}
    ${KDE4_KCMUTILS_LIBS}
    ${Qt5Test_LIBRARIES}
    ${KDE4Support_LIBRARIES}
    ${KCoreAddons_LIBRARIES})

INSTALL(DIRECTORY qml DESTINATION ${DATA_INSTALL_DIR}/kwincompositing)
INSTALL(TARGETS kwincompositing ${INSTALL_TARGETS_DEFAULT_ARGS})
################# list the subdirectories #################

feature_summary(WHAT ALL   FATAL_ON_MISSING_REQUIRED_PACKAGES)
