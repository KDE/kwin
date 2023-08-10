/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "outputbackend.h"

#include "inputbackend.h"
#include "opengl/egldisplay.h"
#include "output.h"
#include "outputconfiguration.h"
#include "platformsupport/scenes/opengl/openglbackend.h"
#include "platformsupport/scenes/qpainter/qpainterbackend.h"
#include "platformsupport/scenes/vulkan/vulkan_backend.h"

namespace KWin
{

OutputBackend::OutputBackend(QObject *parent)
    : QObject(parent)
{
}

OutputBackend::~OutputBackend()
{
}

std::unique_ptr<InputBackend> OutputBackend::createInputBackend()
{
    return nullptr;
}

std::unique_ptr<OpenGLBackend> OutputBackend::createOpenGLBackend()
{
    return nullptr;
}

std::unique_ptr<QPainterBackend> OutputBackend::createQPainterBackend()
{
    return nullptr;
}

std::unique_ptr<VulkanBackend> OutputBackend::createVulkanBackend()
{
    return nullptr;
}

bool OutputBackend::applyOutputChanges(const OutputConfiguration &config)
{
    const auto availableOutputs = outputs();
    QList<Output *> toBeEnabledOutputs;
    QList<Output *> toBeDisabledOutputs;
    for (const auto &output : availableOutputs) {
        if (const auto changeset = config.constChangeSet(output)) {
            if (changeset->enabled) {
                toBeEnabledOutputs << output;
            } else {
                toBeDisabledOutputs << output;
            }
        }
    }
    for (const auto &output : toBeEnabledOutputs) {
        output->applyChanges(config);
    }
    for (const auto &output : toBeDisabledOutputs) {
        output->applyChanges(config);
    }
    return true;
}

Output *OutputBackend::findOutput(const QString &name) const
{
    const auto candidates = outputs();
    for (Output *candidate : candidates) {
        if (candidate->name() == name) {
            return candidate;
        }
    }
    return nullptr;
}

Output *OutputBackend::createVirtualOutput(const QString &name, const QSize &size, double scale)
{
    return nullptr;
}

void OutputBackend::removeVirtualOutput(Output *output)
{
    Q_ASSERT(!output);
}

QString OutputBackend::supportInformation() const
{
    return QStringLiteral("Name: %1\n").arg(metaObject()->className());
}

::EGLContext OutputBackend::sceneEglGlobalShareContext() const
{
    return m_globalShareContext;
}

void OutputBackend::setSceneEglGlobalShareContext(::EGLContext context)
{
    m_globalShareContext = context;
}

Session *OutputBackend::session() const
{
    return nullptr;
}

} // namespace KWin

#include "moc_outputbackend.cpp"
