/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_SCENE_QPAINTER_DRM_BACKEND_H
#define KWIN_SCENE_QPAINTER_DRM_BACKEND_H
#include "qpainterbackend.h"
#include "utils.h"

#include <QObject>
#include <QVector>
#include <QSharedPointer>

#include "dumb_swapchain.h"

namespace KWin
{

class DrmBackend;
class DrmDumbBuffer;
class DrmOutput;
class DrmGpu;

class DrmQPainterBackend : public QPainterBackend
{
    Q_OBJECT
public:
    DrmQPainterBackend(DrmBackend *backend, DrmGpu *gpu);

    QImage *bufferForScreen(int screenId) override;
    QRegion beginFrame(int screenId) override;
    void endFrame(int screenId, const QRegion &damage) override;

private:
    void initOutput(DrmOutput *output);
    struct Output {
        DrmOutput *output;
        QSharedPointer<DumbSwapchain> swapchain;
        DamageJournal damageJournal;
    };
    QVector<Output> m_outputs;
    DrmBackend *m_backend;
    DrmGpu *m_gpu;
};
}

#endif
