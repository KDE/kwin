/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <KDecoration3/Private/DecorationSettingsPrivate>

#include <QObject>

class KConfigGroup;

namespace KWin
{
namespace Decoration
{

class SettingsImpl : public QObject, public KDecoration3::DecorationSettingsPrivate
{
    Q_OBJECT
public:
    explicit SettingsImpl(KDecoration3::DecorationSettings *parent);
    ~SettingsImpl() override;
    bool isAlphaChannelSupported() const override;
    bool isOnAllDesktopsAvailable() const override;
    bool isCloseOnDoubleClickOnMenu() const override;
    KDecoration3::BorderSize borderSize() const override
    {
        return m_borderSize;
    }
    QList<KDecoration3::DecorationButtonType> decorationButtonsLeft() const override
    {
        return m_leftButtons;
    }
    QList<KDecoration3::DecorationButtonType> decorationButtonsRight() const override
    {
        return m_rightButtons;
    }
    QFont font() const override
    {
        return m_font;
    }

private:
    void readSettings();
    QList<KDecoration3::DecorationButtonType> readDecorationButtons(const KConfigGroup &config,
                                                                    const char *key,
                                                                    const QList<KDecoration3::DecorationButtonType> &defaultValue) const;
    QList<KDecoration3::DecorationButtonType> m_leftButtons;
    QList<KDecoration3::DecorationButtonType> m_rightButtons;
    KDecoration3::BorderSize m_borderSize;
    bool m_autoBorderSize = true;
    bool m_closeDoubleClickMenu = false;
    QFont m_font;
};
} // Decoration
} // KWin
