/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "krktexture.h"

namespace KWin
{
class KrkTexturePrivate
{
public:
    KrkTexture::Filtering filtering;
    KrkTexture::WrapMode wrapMode;
};

KrkTexture::KrkTexture(QObject *parent)
    : QObject(parent)
    , d(new KrkTexturePrivate)
{
}

KrkTexture::~KrkTexture()
{
}

KrkTexture::Filtering KrkTexture::filtering() const
{
    return d->filtering;
}

void KrkTexture::setFiltering(Filtering filter)
{
    d->filtering = filter;
}

KrkTexture::WrapMode KrkTexture::wrapMode() const
{
    return d->wrapMode;
}

void KrkTexture::setWrapMode(WrapMode wrapMode)
{
    d->wrapMode = wrapMode;
}

} // namespace KWin
