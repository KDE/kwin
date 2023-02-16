/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "window.h"
#include "pip.h"

Window::Window(QWidget *parent)
    : QWidget(parent)
{
    resize(800, 600);

    m_media = new Media(this);
    m_media->setGeometry(100, 100, 400, 300);
}

#include "moc_window.cpp"
