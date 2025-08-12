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

#include <memory>
#include <optional>

namespace KWin
{

class LogicalOutput;
class InputBackend;
class EglBackend;
class QPainterBackend;
class OutputConfiguration;
class EglDisplay;
class Session;

class KWIN_EXPORT Outputs : public QList<LogicalOutput *>
{
public:
    Outputs(){};
    template<typename T>
    Outputs(const QList<T> &other)
    {
        resize(other.size());
        std::copy(other.constBegin(), other.constEnd(), begin());
    }
};

class KWIN_EXPORT OutputBackend : public QObject
{
    Q_OBJECT
public:
    ~OutputBackend() override;

    virtual bool initialize() = 0;
    virtual std::unique_ptr<InputBackend> createInputBackend();
    virtual std::unique_ptr<EglBackend> createOpenGLBackend();
    virtual std::unique_ptr<QPainterBackend> createQPainterBackend();

    virtual EglDisplay *sceneEglDisplayObject() const = 0;
    /**
     * Returns the compositor-wide shared EGL context. This function may return EGL_NO_CONTEXT
     * if the underlying rendering backend does not use EGL.
     *
     * Note that the returned context should never be made current. Instead, create a context
     * that shares with this one and make the new context current.
     */
    ::EGLContext sceneEglGlobalShareContext() const;
    /**
     * Sets the global share context to @a context. This function is intended to be called only
     * by rendering backends.
     */
    void setSceneEglGlobalShareContext(::EGLContext context);

    /**
     * The CompositingTypes supported by the Platform.
     * The first item should be the most preferred one.
     * @since 5.11
     */
    virtual QList<CompositingType> supportedCompositors() const = 0;

    virtual Outputs outputs() const = 0;
    LogicalOutput *findOutput(const QString &name) const;

    /**
     * A string of information to include in kwin debug output
     * It should not be translated.
     *
     * The base implementation prints the name.
     * @since 5.12
     */
    virtual QString supportInformation() const;

    virtual LogicalOutput *createVirtualOutput(const QString &name, const QString &description, const QSize &size, qreal scale);
    virtual void removeVirtualOutput(LogicalOutput *output);

    /**
     * Applies the output changes. Default implementation only sets values common between platforms
     */
    virtual OutputConfigurationError applyOutputChanges(const OutputConfiguration &config);

    virtual Session *session() const;

Q_SIGNALS:
    void outputsQueried();
    /**
     * This signal is emitted when an output has been connected. The @a output is not ready
     * for compositing yet.
     */
    void outputAdded(LogicalOutput *output);
    /**
     * This signal is emitted when an output has been disconnected.
     */
    void outputRemoved(LogicalOutput *output);

protected:
    explicit OutputBackend(QObject *parent = nullptr);

    ::EGLContext m_globalShareContext = EGL_NO_CONTEXT;
};

} // namespace KWin
