/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023-2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "kwin_export.h"

#include <QHash>
#include <QList>
#include <QString>
#include <unordered_map>
#include <vector>

#include <epoxy/gl.h>
#include <libdrm/drm_fourcc.h>
#include <optional>
#include <stdint.h>
#include <vulkan/vulkan_core.h>

namespace KWin
{

// TODO once FreeBSD is on clang 21+, switch this to use std::flat_set
class KWIN_EXPORT ModifierList : public QList<uint64_t>
{
public:
    ModifierList();
    ModifierList(QList<uint64_t> &&move);
    ModifierList(const QList<uint64_t> &copy);
    ModifierList(const std::initializer_list<uint64_t> &list);

    void insert(uint64_t modifier);
    void erase(uint64_t modifier);
    void insert(const ModifierList &other);

    void intersect(const ModifierList &other);
    ModifierList intersected(const ModifierList &other) const;
};

class KWIN_EXPORT FormatModifierMap : public QHash<uint32_t, ModifierList>
{
public:
    FormatModifierMap();
    FormatModifierMap(QHash<uint32_t, ModifierList> &&move);
    FormatModifierMap(const QHash<uint32_t, ModifierList> &copy);
    FormatModifierMap(const std::initializer_list<std::pair<uint32_t, ModifierList>> &list);

    FormatModifierMap merged(const FormatModifierMap &other) const;
    bool containsFormat(uint32_t format, uint64_t modifier) const;
};

struct YuvFormat
{
    uint32_t format = DRM_FORMAT_YUYV;
    uint32_t widthDivisor = 1;
    uint32_t heightDivisor = 1;
};
struct YuvConversion
{
    QList<struct YuvFormat> plane = {};
};

struct KWIN_EXPORT FormatInfo
{
    uint32_t drmFormat;
    uint32_t bitsPerColor;
    uint32_t alphaBits;
    uint32_t bitsPerPixel;
    GLint openglFormat;
    VkFormat vulkanFormat;
    bool floatingPoint;

    static const QHash<uint32_t, YuvConversion> s_drmConversions;
    static const std::unordered_map<uint32_t, FormatInfo> s_knownFormats;

    std::optional<YuvConversion> yuvConversion() const
    {
        const auto it = s_drmConversions.find(drmFormat);
        return it != s_drmConversions.end() ? *it : std::optional<YuvConversion>{};
    }

    static std::optional<FormatInfo> get(uint32_t drmFormat);
    static QString drmFormatName(uint32_t format);
};

}
