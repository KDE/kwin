/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "libkwineffects/kwinglobals.h"
#include <epoxy/egl.h>
#include <kwin_export.h>

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
class EglDisplay;

struct DmaBufParams
{
    int planeCount = 0;
    int width = 0;
    int height = 0;
    uint32_t format = 0;
    uint64_t modifier = 0;
};

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

    virtual EglDisplay *sceneEglDisplayObject() const = 0;

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

    ::EGLContext m_globalShareContext = EGL_NO_CONTEXT;
};

} // namespace KWin
