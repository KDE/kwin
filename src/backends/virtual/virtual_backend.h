/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/outputbackend.h"

#include <kwin_export.h>

#include <QObject>
#include <QRect>

namespace KWin
{
class VirtualBackend;
class VirtualOutput;

class KWIN_EXPORT VirtualBackend : public OutputBackend
{
    Q_OBJECT

public:
    VirtualBackend(QObject *parent = nullptr);
    ~VirtualBackend() override;

    bool initialize() override;

    std::unique_ptr<QPainterBackend> createQPainterBackend() override;
    std::unique_ptr<OpenGLBackend> createOpenGLBackend() override;

    Output *addOutput(const QSize &size, qreal scale);

    Q_INVOKABLE void setVirtualOutputs(const QVector<QRect> &geometries, QVector<qreal> scales = QVector<qreal>());

    Outputs outputs() const override;

    QVector<CompositingType> supportedCompositors() const override
    {
        return QVector<CompositingType>{OpenGLCompositing, QPainterCompositing};
    }

Q_SIGNALS:
    void virtualOutputsSet(bool countChanged);

private:
    VirtualOutput *createOutput(const QPoint &position, const QSize &size, qreal scale);

    QVector<VirtualOutput *> m_outputs;
};

} // namespace KWin
