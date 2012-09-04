/********************************************************************
Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef KWIN_PLASTIK_BUTTON_H
#define KWIN_PLASTIK_BUTTON_H

#include <QDeclarativeImageProvider>

namespace KWin
{

class PlastikButtonProvider : public QDeclarativeImageProvider
{
public:
    explicit PlastikButtonProvider();
    virtual QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize);

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
        NumButtonIcons
    };
    enum Object {
        HorizontalLine,
        VerticalLine,
        DiagonalLine,
        CrossDiagonalLine
    };
    QPixmap icon(ButtonIcon icon, int size, bool active, bool shadow);
    void drawObject(QPainter &p, Object object, int x, int y, int length, int lineWidth);
};

} // namespace

#endif //  KWIN_PLASTIK_BUTTON_H
