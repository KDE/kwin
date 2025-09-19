# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: 2025 David Redondo <kde@david-redondo.de>

add_executable(kwindowprop main.cpp)
target_link_libraries(kwindowprop Qt::Core Qt::DBus)
install(TARGETS kwindowprop ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

