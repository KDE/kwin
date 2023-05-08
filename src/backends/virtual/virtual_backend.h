/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/outputbackend.h"
#include "utils/filedescriptor.h"

#include <QRect>

struct gbm_device;

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

    struct OutputInfo
    {
        QRect geometry;
        double scale = 1;
    };
    Output *addOutput(const OutputInfo &info);
    void setVirtualOutputs(const QVector<OutputInfo> &infos);

    Outputs outputs() const override;

    QVector<CompositingType> supportedCompositors() const override;

    void setEglDisplay(std::unique_ptr<EglDisplay> &&display);
    EglDisplay *sceneEglDisplayObject() const override;

    gbm_device *gbmDevice() const;

Q_SIGNALS:
    void virtualOutputsSet(bool countChanged);

private:
    VirtualOutput *createOutput(const OutputInfo &info);

    QVector<VirtualOutput *> m_outputs;
    std::unique_ptr<EglDisplay> m_display;
    FileDescriptor m_drmFileDescriptor;
    gbm_device *m_gbmDevice = nullptr;
};

} // namespace KWin
