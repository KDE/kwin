/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "outputlayerdelegate.h"

namespace KWin
{

OutputLayer *OutputLayerDelegate::layer() const
{
    return m_layer;
}

void OutputLayerDelegate::setLayer(OutputLayer *layer)
{
    m_layer = layer;
}

void OutputLayerDelegate::frame(OutputFrame *frame)
{
}

QRegion OutputLayerDelegate::prePaint()
{
    return QRegion();
}

void OutputLayerDelegate::postPaint()
{
}

QList<SurfaceItem *> OutputLayerDelegate::scanoutCandidates(ssize_t maxCount) const
{
    return {};
}

double OutputLayerDelegate::desiredHdrHeadroom() const
{
    return 1;
}

} // namespace KWin
