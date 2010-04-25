/*
    Library for Aurorae window decoration themes.
    Copyright (C) 2009, 2010 Martin Gräßlin <kde@martin-graesslin.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*/

#ifndef AURORAE_AURORAETAB_H
#define AURORAE_AURORAETAB_H

#include <QtGui/QGraphicsWidget>

class QGraphicsDropShadowEffect;
namespace Aurorae {
class AuroraeTheme;

class AuroraeTab : public QGraphicsWidget
{
    Q_OBJECT
public:
    AuroraeTab(AuroraeTheme *theme, const QString &caption);
    virtual ~AuroraeTab();
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);
    void setCaption(const QString &caption);

public Q_SLOTS:
    void activeChanged();

private Q_SLOTS:
    void buttonSizesChanged();

private:
    AuroraeTheme *m_theme;
    QString m_caption;
    QGraphicsDropShadowEffect *m_effect;
};

}

#endif // AURORAE_AURORAETAB_H
