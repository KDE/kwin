/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwin_export.h>

#include <QImage>

#include <memory>

namespace KWin
{

class GLTexture;
class GLTextureUploaderPrivate;

class KWIN_EXPORT GLTextureUploader
{
public:
    GLTextureUploader();
    ~GLTextureUploader();

    void upload(GLTexture *texture, const QImage &source, const QRegion &region);

private:
    std::unique_ptr<GLTextureUploaderPrivate> d;
};

} // namespace KWin
