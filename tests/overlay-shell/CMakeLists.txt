# SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
# SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.20)
project(overlay-shell)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ECM 6.0.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(CheckCXXCompilerFlag)
include(FeatureSummary)
include(FindWaylandScanner)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMFindQmlModule)
include(ECMQmlModule)

find_package(Qt6 CONFIG REQUIRED COMPONENTS
    Core
    Quick
    Test
    Gui
    QuickControls2
    WaylandClientPrivate
)

find_package(KF6 REQUIRED COMPONENTS
    Kirigami
    I18n
    CoreAddons
    QQC2DesktopStyle
    IconThemes
)

ecm_find_qmlmodule(org.kde.kirigami REQUIRED)

# For Wayland_DATADIR.
find_package(Wayland REQUIRED COMPONENTS
    Client
)

find_package(PlasmaWaylandProtocols 1.21.0 CONFIG)
set_package_properties(PlasmaWaylandProtocols PROPERTIES
    TYPE REQUIRED
    PURPOSE "Collection of Plasma-specific Wayland protocols"
    URL "https://invent.kde.org/libraries/plasma-wayland-protocols/"
)

check_cxx_compiler_flag(-Wno-unused-parameter COMPILER_UNUSED_PARAMETER_SUPPORTED)
if (COMPILER_UNUSED_PARAMETER_SUPPORTED)
    add_compile_options(-Wno-unused-parameter)
endif()

add_executable(overlaytest)

ecm_add_qml_module(overlaytest
    URI
    org.kde.overlay
)

target_sources(overlaytest PRIVATE
    main.cpp
    overlayshellsurface.cpp
)

qt6_generate_wayland_protocol_client_sources(overlaytest
    PRIVATE_CODE
    FILES
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/kde-overlay-shell-v1.xml
        ${Wayland_DATADIR}/wayland.xml
)

ecm_target_qml_sources(overlaytest SOURCES
    Main.qml
)

target_link_libraries(overlaytest PRIVATE
    Qt6::Quick
    Qt6::Qml
    Qt6::Gui
    Qt6::QuickControls2
    Qt6::Widgets
    Qt6::WaylandClientPrivate
    KF6::I18n
    KF6::CoreAddons
    KF6::IconThemes
)
