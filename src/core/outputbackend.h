/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/globals.h"
#include <epoxy/egl.h>
#include <kwin_export.h>

#include <QObject>

#include <expected>
#include <memory>
#include <optional>

namespace KWin
{

class LogicalOutput;
class InputBackend;
class EglBackend;
class OutputConfiguration;
class EglDisplay;
class Session;
class BackendOutput;
class EglContext;
class RenderDevice;

class KWIN_EXPORT OutputBackend : public QObject
{
    Q_OBJECT

public:
    ~OutputBackend() override;

    virtual bool initialize() = 0;
    virtual std::unique_ptr<InputBackend> createInputBackend();
    virtual std::unique_ptr<EglBackend> createOpenGLBackend(RenderDevice *device);

    /**
     * The CompositingTypes supported by the Platform.
     * The first item should be the most preferred one.
     * @since 5.11
     */
    virtual QList<CompositingType> supportedCompositors() const = 0;

    virtual QList<BackendOutput *> outputs() const = 0;
    BackendOutput *findOutput(const QString &name) const;

    /**
     * A string of information to include in kwin debug output
     * It should not be translated.
     *
     * The base implementation prints the name.
     * @since 5.12
     */
    virtual QString supportInformation() const;

    virtual BackendOutput *createVirtualOutput(const QString &name, const QString &description, const QSize &size, qreal scale);
    virtual void removeVirtualOutput(BackendOutput *output);

    /**
     * Applies the output changes. Default implementation only sets values common between platforms
     */
    virtual std::expected<void, OutputError> applyOutputChanges(const OutputConfiguration &config);

    virtual Session *session() const;

Q_SIGNALS:
    void outputsQueried();
    /**
     * This signal is emitted when an output has been connected. The @a output is not ready
     * for compositing yet.
     */
    void outputAdded(BackendOutput *output);
    /**
     * This signal is emitted when an output has been disconnected.
     */
    void outputRemoved(BackendOutput *output);

protected:
    explicit OutputBackend(QObject *parent = nullptr);

    EglContext *m_globalShareContext = nullptr;
};

} // namespace KWin
