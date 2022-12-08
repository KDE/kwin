/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwinglobals.h"
#include "rendertarget.h"

#include <QObject>

namespace KWin
{

class Output;
class OverlayWindow;
class OutputLayer;

/**
 * The RenderBackend class is the base class for all rendering backends.
 */
class KWIN_EXPORT RenderBackend : public QObject
{
    Q_OBJECT

public:
    explicit RenderBackend(QObject *parent = nullptr);

    virtual CompositingType compositingType() const = 0;
    virtual OverlayWindow *overlayWindow() const;

    virtual bool checkGraphicsReset();

    virtual OutputLayer *primaryLayer(Output *output) = 0;
    virtual OutputLayer *cursorLayer(Output *output);
    virtual void present(Output *output) = 0;

    virtual QHash<uint32_t, QVector<uint64_t>> supportedFormats() const;
};

} // namespace KWin
