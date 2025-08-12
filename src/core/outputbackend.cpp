/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "outputbackend.h"

#include "inputbackend.h"
#include "opengl/eglbackend.h"
#include "opengl/egldisplay.h"
#include "output.h"
#include "outputconfiguration.h"
#include "qpainter/qpainterbackend.h"

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

std::unique_ptr<EglBackend> OutputBackend::createOpenGLBackend()
{
    return nullptr;
}

std::unique_ptr<QPainterBackend> OutputBackend::createQPainterBackend()
{
    return nullptr;
}

OutputConfigurationError OutputBackend::applyOutputChanges(const OutputConfiguration &config)
{
    const auto availableOutputs = outputs();
    QList<LogicalOutput *> toBeEnabledOutputs;
    QList<LogicalOutput *> toBeDisabledOutputs;
    for (LogicalOutput *output : availableOutputs) {
        if (const auto changeset = config.constChangeSet(output)) {
            if (changeset->enabled.value_or(output->isEnabled())) {
                toBeEnabledOutputs << output;
            } else {
                toBeDisabledOutputs << output;
            }
        }
    }
    for (LogicalOutput *output : toBeEnabledOutputs) {
        output->applyChanges(config);
    }
    for (LogicalOutput *output : toBeDisabledOutputs) {
        output->applyChanges(config);
    }
    return OutputConfigurationError::None;
}

LogicalOutput *OutputBackend::findOutput(const QString &name) const
{
    const auto candidates = outputs();
    for (LogicalOutput *candidate : candidates) {
        if (candidate->name() == name) {
            return candidate;
        }
    }
    return nullptr;
}

LogicalOutput *OutputBackend::createVirtualOutput(const QString &name, const QString &description, const QSize &size, double scale)
{
    return nullptr;
}

void OutputBackend::removeVirtualOutput(LogicalOutput *output)
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
