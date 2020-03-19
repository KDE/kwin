/*
 *
 * Copyright (c) 1998 Matthias Ettrich <ettrich@kde.org>
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

#include "mouse.h"

#include <QLabel>
#include <KComboBox>

#include <QLayout>
#include <QSizePolicy>
#include <QBitmap>

#include <QGroupBox>
#include <QPixmap>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>

#include <QDebug>
#include <kcolorscheme.h>
#include <kseparator.h>
#include <QtDBus>

#include <cstdlib>

#include "kwinoptions_settings.h"


namespace
{

QPixmap maxButtonPixmaps[3];

void createMaxButtonPixmaps()
{
    char const *maxButtonXpms[][3 + 13] = {
        {
            nullptr, nullptr, nullptr,
            "...............",
            ".......#.......",
            "......###......",
            ".....#####.....",
            "..#....#....#..",
            ".##....#....##.",
            "###############",
            ".##....#....##.",
            "..#....#....#..",
            ".....#####.....",
            "......###......",
            ".......#.......",
            "..............."
        },
        {
            nullptr, nullptr, nullptr,
            "...............",
            ".......#.......",
            "......###......",
            ".....#####.....",
            ".......#.......",
            ".......#.......",
            ".......#.......",
            ".......#.......",
            ".......#.......",
            ".....#####.....",
            "......###......",
            ".......#.......",
            "..............."
        },
        {
            nullptr, nullptr, nullptr,
            "...............",
            "...............",
            "...............",
            "...............",
            "..#.........#..",
            ".##.........##.",
            "###############",
            ".##.........##.",
            "..#.........#..",
            "...............",
            "...............",
            "...............",
            "..............."
        },
    };

    QByteArray baseColor(". c " + KColorScheme(QPalette::Active, KColorScheme::View).background().color().name().toLatin1());
    QByteArray textColor("# c " + KColorScheme(QPalette::Active, KColorScheme::View).foreground().color().name().toLatin1());
    for (int t = 0; t < 3; ++t) {
        maxButtonXpms[t][0] = "15 13 2 1";
        maxButtonXpms[t][1] = baseColor.constData();
        maxButtonXpms[t][2] = textColor.constData();
        maxButtonPixmaps[t] = QPixmap(maxButtonXpms[t]);
        maxButtonPixmaps[t].setMask(maxButtonPixmaps[t].createHeuristicMask());
    }
}

} // namespace

KWinMouseConfigForm::KWinMouseConfigForm(QWidget *parent)
    : QWidget(parent)
{
    setupUi(parent);
}

KWinActionsConfigForm::KWinActionsConfigForm(QWidget *parent)
    : QWidget(parent)
{
    setupUi(parent);
}

void KTitleBarActionsConfig::paletteChanged()
{
    createMaxButtonPixmaps();
    for (int i=0; i<3; ++i) {
        m_ui->kcfg_MaximizeButtonLeftClickCommand->setItemIcon(i, maxButtonPixmaps[i]);
        m_ui->kcfg_MaximizeButtonMiddleClickCommand->setItemIcon(i, maxButtonPixmaps[i]);
        m_ui->kcfg_MaximizeButtonRightClickCommand->setItemIcon(i, maxButtonPixmaps[i]);
    }

}

KTitleBarActionsConfig::KTitleBarActionsConfig(bool _standAlone, QWidget *parent)
    : KCModule(parent), standAlone(_standAlone)
    , m_ui(new KWinMouseConfigForm(this))
{
    addConfig(KWinOptionsSettings::self(), this);

    // create the items for the maximize button actions
    createMaxButtonPixmaps();
    for (int i=0; i<3; ++i) {
        m_ui->kcfg_MaximizeButtonLeftClickCommand->addItem(maxButtonPixmaps[i], QString());
        m_ui->kcfg_MaximizeButtonMiddleClickCommand->addItem(maxButtonPixmaps[i], QString());
        m_ui->kcfg_MaximizeButtonRightClickCommand->addItem(maxButtonPixmaps[i], QString());
    }
    createMaximizeButtonTooltips(m_ui->kcfg_MaximizeButtonLeftClickCommand);
    createMaximizeButtonTooltips(m_ui->kcfg_MaximizeButtonMiddleClickCommand);
    createMaximizeButtonTooltips(m_ui->kcfg_MaximizeButtonRightClickCommand);

    load();
}

void KTitleBarActionsConfig::createMaximizeButtonTooltips(KComboBox *combo)
{
    combo->setItemData(0, i18n("Maximize"), Qt::ToolTipRole);
    combo->setItemData(1, i18n("Maximize (vertical only)"), Qt::ToolTipRole);
    combo->setItemData(2, i18n("Maximize (horizontal only)"), Qt::ToolTipRole);
}

void KTitleBarActionsConfig::showEvent(QShowEvent *ev)
{
    if (!standAlone) {
        // Workaround KCModule::showEvent() calling load(), see bug 163817
        QWidget::showEvent(ev);
        return;
    }
    KCModule::showEvent(ev);
}

void KTitleBarActionsConfig::changeEvent(QEvent *ev)
{
    if (ev->type() == QEvent::PaletteChange) {
        paletteChanged();
    }
    ev->accept();
}

void KTitleBarActionsConfig::save()
{
    KCModule::save();

    if (standAlone) {
        // Send signal to all kwin instances
        QDBusMessage message =
            QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
        QDBusConnection::sessionBus().send(message);
    }
}

KWindowActionsConfig::KWindowActionsConfig(bool _standAlone, QWidget *parent)
    : KCModule(parent), standAlone(_standAlone)
    , m_ui(new KWinActionsConfigForm(this))
{
    addConfig(KWinOptionsSettings::self(), this);
    load();
}

void KWindowActionsConfig::showEvent(QShowEvent *ev)
{
    if (!standAlone) {
        QWidget::showEvent(ev);
        return;
    }
    KCModule::showEvent(ev);
}

void KWindowActionsConfig::save()
{
    KCModule::save();

    if (standAlone) {
        // Send signal to all kwin instances
        QDBusMessage message =
            QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
        QDBusConnection::sessionBus().send(message);
    }
}
