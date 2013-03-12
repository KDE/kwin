/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2009 Martin Gräßlin <mgraesslin@kde.org>

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

#ifndef DESKTOPNAMESWIDGET_H
#define DESKTOPNAMESWIDGET_H

#include <QWidget>
#include <QList>

class KLineEdit;
class QLabel;
class QGridLayout;

namespace KWin
{
class KWinDesktopConfig;

class DesktopNamesWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DesktopNamesWidget(QWidget *parent);
    ~DesktopNamesWidget();
    QString name(int desktop);
    void setName(int desktop, QString desktopName);
    void setDefaultName(int desktop);
    void setMaxDesktops(int maxDesktops);
    void setDesktopConfig(KWinDesktopConfig *desktopConfig);

signals:
    void changed();

public slots:
    void numberChanged(int number);

private:
    QList< QLabel* > m_nameLabels;
    QList< KLineEdit* > m_nameInputs;
    QGridLayout* m_namesLayout;
    int m_maxDesktops;
    KWinDesktopConfig *m_desktopConfig;
};

} // namespace

#endif // DESKTOPNAMESWIDGET_H
