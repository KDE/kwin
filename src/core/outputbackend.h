/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <epoxy/egl.h>
#include <kwin_export.h>
#include <kwinglobals.h>

#include <QObject>

#include <memory>
#include <optional>

namespace KWin
{

class Output;
class DmaBufTexture;
class InputBackend;
class OpenGLBackend;
class QPainterBackend;
class OutputConfiguration;
struct DmaBufParams;

class KWIN_EXPORT Outputs : public QVector<Output *>
{
public:
    Outputs(){};
    template<typename T>
    Outputs(const QVector<T> &other)
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
    virtual std::unique_ptr<OpenGLBackend> createOpenGLBackend();
    virtual std::unique_ptr<QPainterBackend> createQPainterBackend();
    virtual std::optional<DmaBufParams> testCreateDmaBuf(const QSize &size, quint32 format, const QVector<uint64_t> &modifiers);
    virtual std::shared_ptr<DmaBufTexture> createDmaBufTexture(const QSize &size, quint32 format, const uint64_t modifier);
    std::shared_ptr<DmaBufTexture> createDmaBufTexture(const DmaBufParams &attributes);

    /**
     * The EGLDisplay used by the compositing scene.
     */
    EGLDisplay sceneEglDisplay() const;
    void setSceneEglDisplay(EGLDisplay display);
    /**
     * Returns the compositor-wide shared EGL context. This function may return EGL_NO_CONTEXT
     * if the underlying rendering backend does not use EGL.
     *
     * Note that the returned context should never be made current. Instead, create a context
     * that shares with this one and make the new context current.
     */
    EGLContext sceneEglGlobalShareContext() const;
    /**
     * Sets the global share context to @a context. This function is intended to be called only
     * by rendering backends.
     */
    void setSceneEglGlobalShareContext(EGLContext context);

    /**
     * The CompositingTypes supported by the Platform.
     * The first item should be the most preferred one.
     * @since 5.11
     */
    virtual QVector<CompositingType> supportedCompositors() const = 0;

    virtual Outputs outputs() const = 0;
    Output *findOutput(const QString &name) const;

    /**
     * A string of information to include in kwin debug output
     * It should not be translated.
     *
     * The base implementation prints the name.
     * @since 5.12
     */
    virtual QString supportInformation() const;

    virtual Output *createVirtualOutput(const QString &name, const QSize &size, qreal scale);
    virtual void removeVirtualOutput(Output *output);

    /**
     * Applies the output changes. Default implementation only sets values common between platforms
     */
    virtual bool applyOutputChanges(const OutputConfiguration &config);

public Q_SLOTS:
    virtual void sceneInitialized(){};

Q_SIGNALS:
    void outputsQueried();
    /**
     * This signal is emitted when an output has been connected. The @a output is not ready
     * for compositing yet.
     */
    void outputAdded(Output *output);
    /**
     * This signal is emitted when an output has been disconnected.
     */
    void outputRemoved(Output *output);

protected:
    explicit OutputBackend(QObject *parent = nullptr);

private:
    EGLDisplay m_eglDisplay;
    EGLContext m_globalShareContext = EGL_NO_CONTEXT;
};

} // namespace KWin
