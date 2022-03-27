/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QHash>
#include <QObject>

namespace KWin
{

class GLRenderTarget;
class AbstractOutput;
class RenderOutput;
class GLTexture;

class ScreenCastSource : public QObject
{
    Q_OBJECT

public:
    explicit ScreenCastSource(QObject *parent = nullptr);

    virtual bool hasAlphaChannel() const = 0;
    virtual QSize textureSize() const = 0;

    virtual void render(GLRenderTarget *target) = 0;
    virtual void render(QImage *image) = 0;
    virtual std::chrono::nanoseconds clock() const = 0;

Q_SIGNALS:
    void closed();

protected:
    QHash<RenderOutput *, QSharedPointer<GLTexture>> getTextures(AbstractOutput *output) const;
};

} // namespace KWin
