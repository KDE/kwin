/*
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_PLASTIK_BUTTON_H
#define KWIN_PLASTIK_BUTTON_H

#include <QQuickImageProvider>

namespace KWin
{

class PlastikButtonProvider : public QQuickImageProvider
{
public:
    explicit PlastikButtonProvider();
    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    enum ButtonIcon {
        CloseIcon = 0,
        MaxIcon,
        MaxRestoreIcon,
        MinIcon,
        HelpIcon,
        OnAllDesktopsIcon,
        NotOnAllDesktopsIcon,
        KeepAboveIcon,
        NoKeepAboveIcon,
        KeepBelowIcon,
        NoKeepBelowIcon,
        ShadeIcon,
        UnShadeIcon,
        AppMenuIcon,
        NumButtonIcons
    };
    enum Object {
        HorizontalLine,
        VerticalLine,
        DiagonalLine,
        CrossDiagonalLine
    };
    enum DecorationButton {
        /**
         * Invalid button value. A decoration should not create a button for
         * this type.
         */
        DecorationButtonNone,
        DecorationButtonMenu,
        DecorationButtonApplicationMenu,
        DecorationButtonOnAllDesktops,
        DecorationButtonQuickHelp,
        DecorationButtonMinimize,
        DecorationButtonMaximizeRestore,
        DecorationButtonClose,
        DecorationButtonKeepAbove,
        DecorationButtonKeepBelow,
        DecorationButtonShade,
        DecorationButtonResize,
        /**
         * The decoration should create an empty spacer instead of a button for
         * this type.
         */
        DecorationButtonExplicitSpacer
    };
    QPixmap icon(ButtonIcon icon, int size, bool active, bool shadow);
    void drawObject(QPainter &p, Object object, int x, int y, int length, int lineWidth);
};

} // namespace

#endif //  KWIN_PLASTIK_BUTTON_H
