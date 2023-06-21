/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
// vulkan.hpp requires some defines set before you include it
// include this header instead of vulkan.hpp to have that done in a unified way
#define VULKAN_HPP_NO_EXCEPTIONS 1
#define VULKAN_HPP_ASSERT_ON_RESULT void
#include <vulkan/vulkan.hpp>

#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(KWIN_VULKAN)
