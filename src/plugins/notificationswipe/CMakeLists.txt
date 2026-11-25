# SPDX-FileCopyrightText: 2026 Artem Grinev <agrinev98@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

kcoreaddons_add_plugin(NotificationSwipePlugin INSTALL_NAMESPACE "kwin/plugins")

target_sources(NotificationSwipePlugin PRIVATE
    main.cpp
    notificationdrag.cpp
    notificationswipe.cpp
    touchpadswipefilter.cpp
)
target_link_libraries(NotificationSwipePlugin PRIVATE kwin)
