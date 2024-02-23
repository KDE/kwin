# SPDX-FileCopyrightText: None
# SPDX-License-Identifier: CC0-1.0

cmake_minimum_required(VERSION 3.20)
project(quick-effect)

set(KF6_MIN_VERSION "6.0.0")

find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS
    Package
)

kpackage_install_package(package quick-effect effects kwin)
