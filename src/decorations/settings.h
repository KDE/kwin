/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <KDecoration2/Private/DecorationSettingsPrivate>

#include <QObject>

class KConfigGroup;

namespace KWin
{
namespace Decoration
{

class SettingsImpl : public QObject, public KDecoration2::DecorationSettingsPrivate
{
    Q_OBJECT
public:
    explicit SettingsImpl(KDecoration2::DecorationSettings *parent);
    ~SettingsImpl() override;
    bool isAlphaChannelSupported() const override;
    bool isOnAllDesktopsAvailable() const override;
    bool isCloseOnDoubleClickOnMenu() const override;
    KDecoration2::BorderSize borderSize() const override
    {
        return m_borderSize;
    }
    QVector<KDecoration2::DecorationButtonType> decorationButtonsLeft() const override
    {
        return m_leftButtons;
    }
    QVector<KDecoration2::DecorationButtonType> decorationButtonsRight() const override
    {
        return m_rightButtons;
    }
    QFont font() const override
    {
        return m_font;
    }

private:
    void readSettings();
    QVector<KDecoration2::DecorationButtonType> readDecorationButtons(const KConfigGroup &config,
                                                                      const char *key,
                                                                      const QVector<KDecoration2::DecorationButtonType> &defaultValue) const;
    QVector<KDecoration2::DecorationButtonType> m_leftButtons;
    QVector<KDecoration2::DecorationButtonType> m_rightButtons;
    KDecoration2::BorderSize m_borderSize;
    bool m_autoBorderSize = true;
    bool m_closeDoubleClickMenu = false;
    QFont m_font;
};
} // Decoration
} // KWin
