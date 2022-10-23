/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/platform.h"

#include <kwin_export.h>

#include <QObject>
#include <QRect>

class QTemporaryDir;

namespace KWin
{
class VirtualBackend;
class VirtualOutput;

class KWIN_EXPORT VirtualBackend : public Platform
{
    Q_OBJECT

public:
    VirtualBackend(QObject *parent = nullptr);
    ~VirtualBackend() override;

    bool initialize() override;

    bool saveFrames() const
    {
        return m_screenshotDir != nullptr;
    }
    QString screenshotDirPath() const;

    std::unique_ptr<QPainterBackend> createQPainterBackend() override;
    std::unique_ptr<OpenGLBackend> createOpenGLBackend() override;

    Q_INVOKABLE void setVirtualOutputs(int count, QVector<QRect> geometries = QVector<QRect>(), QVector<int> scales = QVector<int>());

    Outputs outputs() const override;

    QVector<CompositingType> supportedCompositors() const override
    {
        return QVector<CompositingType>{OpenGLCompositing, QPainterCompositing};
    }

    Q_INVOKABLE QImage captureOutput(Output *output) const;

Q_SIGNALS:
    void virtualOutputsSet(bool countChanged);

private:
    QVector<VirtualOutput *> m_outputs;
    std::unique_ptr<QTemporaryDir> m_screenshotDir;
};

} // namespace KWin
