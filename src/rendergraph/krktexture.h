/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwinglobals.h>

#include <QObject>
#include <QSize>

namespace KWin
{
class KrkTexturePrivate;

namespace KrkNative
{
class KrkNativeTexture;
}

/**
 * The KrkTexture class represents a platform-independent scene graph texture.
 */
class KWIN_EXPORT KrkTexture : public QObject
{
    Q_OBJECT

public:
    enum Filtering {
        Nearest,
        Linear,
    };

    enum WrapMode {
        Repeat,
        ClampToEdge,
    };

    explicit KrkTexture(QObject *parent = nullptr);
    ~KrkTexture() override;

    Filtering filtering() const;
    void setFiltering(Filtering filter);

    WrapMode wrapMode() const;
    void setWrapMode(WrapMode wrapMode);

    virtual void bind() = 0;

    virtual bool hasAlphaChannel() const = 0;
    virtual KrkNative::KrkNativeTexture *nativeTexture() const = 0;

private:
    QScopedPointer<KrkTexturePrivate> d;
};

} // namespace KWin
