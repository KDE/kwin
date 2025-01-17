/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/output.h"
#include "core/outputbackend.h"
#include "utils/filedescriptor.h"

#include <QRect>

namespace KWin
{
class VirtualBackend;
class VirtualOutput;
class DrmDevice;

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
        bool internal = false;
        QSize physicalSizeInMM;
        QList<std::tuple<QSize, uint64_t, OutputMode::Flags>> modes;
        OutputTransform panelOrientation = OutputTransform::Kind::Normal;
        QByteArray edid;
        std::optional<QByteArray> edidIdentifierOverride;
        std::optional<QString> connectorName;
        std::optional<QByteArray> mstPath;
    };
    Output *addOutput(const OutputInfo &info);
    void setVirtualOutputs(const QList<OutputInfo> &infos);

    Outputs outputs() const override;

    QList<CompositingType> supportedCompositors() const override;

    void setEglDisplay(std::unique_ptr<EglDisplay> &&display);
    EglDisplay *sceneEglDisplayObject() const override;

    DrmDevice *drmDevice() const;

Q_SIGNALS:
    void virtualOutputsSet(bool countChanged);

private:
    VirtualOutput *createOutput(const OutputInfo &info);

    QList<VirtualOutput *> m_outputs;
    std::unique_ptr<DrmDevice> m_drmDevice;
    std::unique_ptr<EglDisplay> m_display;
};

} // namespace KWin
