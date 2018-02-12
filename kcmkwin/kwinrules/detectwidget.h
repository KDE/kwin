/*
 * Copyright (c) 2004 Lubos Lunak <l.lunak@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#ifndef __DETECTWIDGET_H__
#define __DETECTWIDGET_H__

#include <QDialog>
#include <kwindowsystem.h>

#include "../../rules.h"
//Added by qt3to4:
#include <QEvent>
#include <QByteArray>

#include "ui_detectwidget.h"

namespace KWin
{

class DetectWidget
    : public QWidget, public Ui_DetectWidget
{
    Q_OBJECT
public:
    explicit DetectWidget(QWidget* parent = nullptr);
};

class DetectDialog
    : public QDialog
{
    Q_OBJECT
public:
    explicit DetectDialog(QWidget* parent = nullptr, const char* name = nullptr);
    void detect(int secs = 0);
    QByteArray selectedClass() const;
    bool selectedWholeClass() const;
    QByteArray selectedRole() const;
    bool selectedWholeApp() const;
    NET::WindowType selectedType() const;
    QString selectedTitle() const;
    Rules::StringMatch titleMatch() const;
    QByteArray selectedMachine() const;

    const QVariantMap &windowInfo() const {
        return m_windowInfo;
    }

Q_SIGNALS:
    void detectionDone(bool);
private Q_SLOTS:
    void selectWindow();
private:
    void executeDialog();
    QByteArray wmclass_class;
    QByteArray wmclass_name;
    QByteArray role;
    NET::WindowType type;
    QString title;
    QByteArray extrarole;
    QByteArray machine;
    DetectWidget* widget;
    QVariantMap m_windowInfo;
};

} // namespace

#endif
