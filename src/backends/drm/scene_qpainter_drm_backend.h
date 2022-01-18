/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_SCENE_QPAINTER_DRM_BACKEND_H
#define KWIN_SCENE_QPAINTER_DRM_BACKEND_H
#include "qpainterbackend.h"
#include "utils/common.h"

#include <QObject>
#include <QVector>
#include <QSharedPointer>

#include "dumb_swapchain.h"

namespace KWin
{

class DrmBackend;
class DrmDumbBuffer;
class DrmAbstractOutput;
class DrmGpu;

class DrmQPainterBackend : public QPainterBackend
{
    Q_OBJECT
public:
    DrmQPainterBackend(DrmBackend *backend, DrmGpu *gpu);

    QImage *bufferForScreen(AbstractOutput *output) override;
    QRegion beginFrame(AbstractOutput *output) override;
    void endFrame(AbstractOutput *output, const QRegion &renderedRegion, const QRegion &damagedRegion) override;

private:
    void initOutput(DrmAbstractOutput *output);
    struct Output {
        DrmAbstractOutput *output;
        QSharedPointer<DumbSwapchain> swapchain;
        DamageJournal damageJournal;
    };
    QMap<AbstractOutput *, Output> m_outputs;
    DrmBackend *m_backend;
    DrmGpu *m_gpu;
};
}

#endif
