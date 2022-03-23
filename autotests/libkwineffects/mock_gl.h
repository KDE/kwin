/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef MOCK_GL_H
#define MOCK_GL_H

#include <QByteArray>
#include <QVector>

struct MockGL
{
    struct
    {
        QByteArray vendor;
        QByteArray renderer;
        QByteArray version;
        QVector<QByteArray> extensions;
        QByteArray extensionsString;
        QByteArray shadingLanguageVersion;
    } getString;
};

extern MockGL *s_gl;

#endif
