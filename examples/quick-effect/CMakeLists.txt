# SPDX-FileCopyrightText: None
# SPDX-License-Identifier: CC0-1.0

cmake_minimum_required(VERSION 3.16)
project(quick-effect)

find_package(ECM 5.240 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

find_package(KF6 5.240 REQUIRED COMPONENTS
    Package
)

kpackage_install_package(package quick-effect effects kwin)
