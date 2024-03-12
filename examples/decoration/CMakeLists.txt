# SPDX-FileCopyrightText: Vlad Zahorodnii <vlad.zahorodnii@kde.org>
# SPDX-License-Identifier: CC0-1.0

cmake_minimum_required(VERSION 3.20)
project(decoration)

set(KF6_MIN_VERSION "6.0.0")

find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(FeatureSummary)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt6 CONFIG REQUIRED COMPONENTS
    Core
)

find_package(KF6 REQUIRED COMPONENTS
    CoreAddons
)

find_package(KDecoration2 REQUIRED)

kcoreaddons_add_plugin(basic INSTALL_NAMESPACE "org.kde.kdecoration2")
target_sources(basic PRIVATE
    main.cpp
    closebutton.cpp
    decoration.cpp
    maximizebutton.cpp
    minimizebutton.cpp
)

target_link_libraries(basic PRIVATE
    Qt6::Core
    Qt6::Gui
    KF6::CoreAddons
    KDecoration2::KDecoration
)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
