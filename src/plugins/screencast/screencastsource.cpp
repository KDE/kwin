/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "screencastsource.h"

#include "composite.h"
#include "kwinglutils.h"
#include "main.h"
#include "platform.h"
#include "renderoutput.h"
#include "scene.h"

namespace KWin
{

ScreenCastSource::ScreenCastSource(QObject *parent)
    : QObject(parent)
{
}

QHash<RenderOutput *, QSharedPointer<GLTexture>> ScreenCastSource::getTextures(AbstractOutput *output) const
{
    const auto outputs = kwinApp()->platform()->renderOutputs();
    QHash<RenderOutput *, QSharedPointer<GLTexture>> textures;
    for (const auto &renderOutput : outputs) {
        if (renderOutput->platformOutput() == output) {
            const auto texture = Compositor::self()->scene()->textureForOutput(renderOutput);
            if (!texture) {
                return {};
            }
            textures.insert(renderOutput, texture);
        }
    }
    return textures;
}

} // namespace KWin
