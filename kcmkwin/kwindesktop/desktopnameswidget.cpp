/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

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

#include "desktopnameswidget.h"
#include "main.h"

#include <QLabel>
#include <QGridLayout>

#include <KLocale>
#include <KLineEdit>

namespace KWin
{

DesktopNamesWidget::DesktopNamesWidget(QWidget *parent)
    : QWidget(parent)
    , m_maxDesktops(0)
{
    m_namesLayout = new QGridLayout;
    m_namesLayout->setMargin(0);

    setLayout(m_namesLayout);
}

DesktopNamesWidget::~DesktopNamesWidget()
{
}

void DesktopNamesWidget::numberChanged(int number)
{
    if ((number < 1) || (number > m_maxDesktops))
        return;
    if (m_nameInputs.size() != number) {
        if (number < m_nameInputs.size()) {
            // remove widgets
            while (number != m_nameInputs.size()) {
                KLineEdit* edit = m_nameInputs.last();
                m_nameInputs.removeLast();
                delete edit;
                QLabel* label = m_nameLabels.last();
                m_nameLabels.removeLast();
                delete label;
            }
        } else {
            // add widgets
            while (number != m_nameInputs.size()) {
                int desktop = m_nameInputs.size();
                QLabel* label = new QLabel(i18n("Desktop %1:", desktop + 1), this);
                KLineEdit* edit = new KLineEdit(this);
                label->setWhatsThis(i18n("Here you can enter the name for desktop %1", desktop + 1));
                edit->setWhatsThis(i18n("Here you can enter the name for desktop %1", desktop + 1));

                m_namesLayout->addWidget(label, desktop % 10, 0 + 2 *(desktop >= 10), 1, 1);
                m_namesLayout->addWidget(edit, desktop % 10, 1 + 2 *(desktop >= 10), 1, 1);
                m_nameInputs << edit;
                m_nameLabels << label;

                setDefaultName(desktop + 1);
                if (desktop > 1) {
                    setTabOrder(m_nameInputs[desktop - 1], m_nameInputs[desktop]);
                }
                connect(edit, SIGNAL(textChanged(QString)), SIGNAL(changed()));
            }
        }
    }
}

QString DesktopNamesWidget::name(int desktop)
{
    if ((desktop < 1) || (desktop > m_maxDesktops) || (desktop > m_nameInputs.size()))
        return QString();
    return m_nameInputs[ desktop -1 ]->text();
}


void DesktopNamesWidget::setName(int desktop, QString desktopName)
{
    if ((desktop < 1) || (desktop > m_maxDesktops) || (desktop > m_nameInputs.size()))
        return;
    m_nameInputs[ desktop-1 ]->setText(desktopName);
}

void DesktopNamesWidget::setDefaultName(int desktop)
{
    if ((desktop < 1) || (desktop > m_maxDesktops))
        return;
    QString name = m_desktopConfig->cachedDesktopName(desktop);
    if (name.isEmpty())
        name = i18n("Desktop %1", desktop);
    m_nameInputs[ desktop -1 ]->setText(name);
}


void DesktopNamesWidget::setMaxDesktops(int maxDesktops)
{
    m_maxDesktops = maxDesktops;
}

void DesktopNamesWidget::setDesktopConfig(KWinDesktopConfig* desktopConfig)
{
    m_desktopConfig = desktopConfig;
}

} // namespace

#include "desktopnameswidget.moc"
