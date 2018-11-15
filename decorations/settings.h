/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef KWIN_DECORATION_SETTINGS_H
#define KWIN_DECORATION_SETTINGS_H

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
    virtual ~SettingsImpl();
    bool isAlphaChannelSupported() const override;
    bool isOnAllDesktopsAvailable() const override;
    bool isCloseOnDoubleClickOnMenu() const override;
    KDecoration2::BorderSize borderSize() const override {
        return m_borderSize;
    }
    QVector< KDecoration2::DecorationButtonType > decorationButtonsLeft() const override {
        return m_leftButtons;
    }
    QVector< KDecoration2::DecorationButtonType > decorationButtonsRight() const override {
        return m_rightButtons;
    }
    QFont font() const override {
        return m_font;
    }

private:
    void readSettings();
    QVector< KDecoration2::DecorationButtonType > readDecorationButtons(const KConfigGroup &config,
                                                                      const char *key,
                                                                      const QVector< KDecoration2::DecorationButtonType > &defaultValue) const;
    QVector< KDecoration2::DecorationButtonType > m_leftButtons;
    QVector< KDecoration2::DecorationButtonType > m_rightButtons;
    KDecoration2::BorderSize m_borderSize;
    bool m_closeDoubleClickMenu = false;
    QFont m_font;
};
} // Decoration
} // KWin

#endif
