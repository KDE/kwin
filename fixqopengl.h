/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Bhushan Shah <bshah@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef FIXQOPENGL_H
#define FIXQOPENGL_H

// qopengl.h declares GLdouble as a typedef of float when Qt is built
// with GLES support.  This conflicts with the epoxy/gl_generated.h
// declaration, so we have to prevent the Qt header from being #included.
#define QOPENGL_H

#ifndef QOPENGLF_APIENTRY
#define QOPENGLF_APIENTRY GLAPIENTRY
#endif

#ifndef QOPENGLF_APIENTRYP
#define QOPENGLF_APIENTRYP GLAPIENTRYP
#endif

#endif //FIXQOPENGL_H
