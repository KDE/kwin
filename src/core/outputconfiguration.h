/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "kwin_export.h"

#include "output.h"

#include <QPoint>
#include <QSize>

namespace KWin
{

class KWIN_EXPORT OutputChangeSet
{
public:
    std::optional<std::weak_ptr<OutputMode>> mode;
    std::optional<bool> enabled;
    std::optional<QPoint> pos;
    std::optional<double> scale;
    std::optional<OutputTransform> transform;
    std::optional<uint32_t> overscan;
    std::optional<Output::RgbRange> rgbRange;
    std::optional<RenderLoop::VrrPolicy> vrrPolicy;
    std::optional<bool> highDynamicRange;
    std::optional<uint32_t> sdrBrightness;
    std::optional<bool> wideColorGamut;
    std::optional<Output::AutoRotationPolicy> autoRotationPolicy;
};

class KWIN_EXPORT OutputConfiguration
{
public:
    std::shared_ptr<OutputChangeSet> changeSet(Output *output);
    std::shared_ptr<OutputChangeSet> constChangeSet(Output *output) const;

private:
    QMap<Output *, std::shared_ptr<OutputChangeSet>> m_properties;
};

}
