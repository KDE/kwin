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

#include <kconfig.h>
#include <kdialog.h>
#include <kdebug.h>
#include <kglobalsettings.h>
#include <kcolorscheme.h>
#include <kseparator.h>
#include <QtDBus/QtDBus>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <stdlib.h>

#include "mouse.moc"

namespace
{

char const * const cnf_Max[] = {
    "MaximizeButtonLeftClickCommand",
    "MaximizeButtonMiddleClickCommand",
    "MaximizeButtonRightClickCommand",
};

char const * const tbl_Max[] = {
    "Maximize",
    "Maximize (vertical only)",
    "Maximize (horizontal only)",
    ""
};

QPixmap maxButtonPixmaps[3];

void createMaxButtonPixmaps()
{
    char const * maxButtonXpms[][3 + 13] = {
        {
            0, 0, 0,
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
            0, 0, 0,
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
            0, 0, 0,
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

    QByteArray baseColor(". c " + KColorScheme(QPalette::Active, KColorScheme::View).background().color().name().toAscii());
    QByteArray textColor("# c " + KColorScheme(QPalette::Active, KColorScheme::View).foreground().color().name().toAscii());
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
    setupUi(this);
}

KWinActionsConfigForm::KWinActionsConfigForm(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);
}

void KTitleBarActionsConfig::paletteChanged()
{
    createMaxButtonPixmaps();
    for (int i=0; i<3; ++i) {
        m_ui->leftClickMaximizeButton->setItemIcon(i, maxButtonPixmaps[i]);
        m_ui->middleClickMaximizeButton->setItemIcon(i, maxButtonPixmaps[i]);
        m_ui->rightClickMaximizeButton->setItemIcon(i, maxButtonPixmaps[i]);
    }

}

KTitleBarActionsConfig::KTitleBarActionsConfig(bool _standAlone, KConfig *_config, const KComponentData &inst, QWidget * parent)
    : KCModule(inst, parent), config(_config), standAlone(_standAlone)
    , m_ui(new KWinMouseConfigForm(this))
{
    // create the items for the maximize button actions
    createMaxButtonPixmaps();
    for (int i=0; i<3; ++i) {
        m_ui->leftClickMaximizeButton->addItem(maxButtonPixmaps[i], QString());
        m_ui->middleClickMaximizeButton->addItem(maxButtonPixmaps[i], QString());
        m_ui->rightClickMaximizeButton->addItem(maxButtonPixmaps[i], QString());
    }
    createMaximizeButtonTooltips(m_ui->leftClickMaximizeButton);
    createMaximizeButtonTooltips(m_ui->middleClickMaximizeButton);
    createMaximizeButtonTooltips(m_ui->rightClickMaximizeButton);

    connect(m_ui->coTiDbl, SIGNAL(activated(int)), SLOT(changed()));
    connect(m_ui->coTiAct1, SIGNAL(activated(int)), SLOT(changed()));
    connect(m_ui->coTiAct2, SIGNAL(activated(int)), SLOT(changed()));
    connect(m_ui->coTiAct3, SIGNAL(activated(int)), SLOT(changed()));
    connect(m_ui->coTiAct4, SIGNAL(activated(int)), SLOT(changed()));
    connect(m_ui->coTiInAct1, SIGNAL(activated(int)), SLOT(changed()));
    connect(m_ui->coTiInAct2, SIGNAL(activated(int)), SLOT(changed()));
    connect(m_ui->coTiInAct3, SIGNAL(activated(int)), SLOT(changed()));
    connect(m_ui->leftClickMaximizeButton, SIGNAL(activated(int)), SLOT(changed()));
    connect(m_ui->middleClickMaximizeButton, SIGNAL(activated(int)), SLOT(changed()));
    connect(m_ui->rightClickMaximizeButton, SIGNAL(activated(int)), SLOT(changed()));
    connect(KGlobalSettings::self(), SIGNAL(kdisplayPaletteChanged()), SLOT(paletteChanged()));

    load();
}

KTitleBarActionsConfig::~KTitleBarActionsConfig()
{
    if (standAlone)
        delete config;
}

void KTitleBarActionsConfig::createMaximizeButtonTooltips(KComboBox *combo)
{
    combo->setItemData(0, i18n("Maximize"), Qt::ToolTipRole);
    combo->setItemData(1, i18n("Maximize (vertical only)"), Qt::ToolTipRole);
    combo->setItemData(2, i18n("Maximize (horizontal only)"), Qt::ToolTipRole);
}

// do NOT change the texts below, they are written to config file
// and are not shown in the GUI
// they have to match the order of items in GUI elements though
const char* const tbl_TiDbl[] = {
    "Maximize",
    "Maximize (vertical only)",
    "Maximize (horizontal only)",
    "Minimize",
    "Shade",
    "Lower",
    "Close",
    "OnAllDesktops",
    "Nothing",
    ""
};

const char* const tbl_TiAc[] = {
    "Raise",
    "Lower",
    "Toggle raise and lower",
    "Minimize",
    "Shade",
    "Close",
    "Operations menu",
    "Start window tab drag",
    "Nothing",
    ""
};

const char* const tbl_TiInAc[] = {
    "Activate and raise",
    "Activate and lower",
    "Activate",
    "Raise",
    "Lower",
    "Toggle raise and lower",
    "Minimize",
    "Shade",
    "Close",
    "Operations menu",
    "Start window tab drag",
    "Nothing",
    ""
};

const char* const tbl_Win[] = {
    "Activate, raise and pass click",
    "Activate and pass click",
    "Activate",
    "Activate and raise",
    ""
};

const char* const tbl_WinWheel[] = {
    "Scroll",
    "Activate and scroll",
    "Activate, raise and scroll",
    ""
};

const char* const tbl_AllKey[] = {
    "Meta",
    "Alt",
    ""
};

const char* const tbl_All[] = {
    "Move",
    "Activate, raise and move",
    "Toggle raise and lower",
    "Resize",
    "Raise",
    "Lower",
    "Minimize",
    "Decrease Opacity",
    "Increase Opacity",
    "Nothing",
    ""
};

const char* tbl_TiWAc[] = {
    "Raise/Lower",
    "Shade/Unshade",
    "Maximize/Restore",
    "Above/Below",
    "Previous/Next Desktop",
    "Change Opacity",
    "Switch to Window Tab to the Left/Right",
    "Nothing",
    ""
};

const char* tbl_AllW[] = {
    "Raise/Lower",
    "Shade/Unshade",
    "Maximize/Restore",
    "Above/Below",
    "Previous/Next Desktop",
    "Change Opacity",
    "Switch to Window Tab to the Left/Right",
    "Nothing",
    ""
};

static const char* tbl_num_lookup(const char* const arr[], int pos)
{
    for (int i = 0;
            arr[ i ][ 0 ] != '\0' && pos >= 0;
            ++i) {
        if (pos == 0)
            return arr[ i ];
        --pos;
    }
    abort(); // should never happen this way
}

static int tbl_txt_lookup(const char* const arr[], const char* txt)
{
    int pos = 0;
    for (int i = 0;
            arr[ i ][ 0 ] != '\0';
            ++i) {
        if (qstricmp(txt, arr[ i ]) == 0)
            return pos;
        ++pos;
    }
    return 0;
}

void KTitleBarActionsConfig::setComboText(KComboBox* combo, const char*txt)
{
    if (combo == m_ui->coTiDbl)
        combo->setCurrentIndex(tbl_txt_lookup(tbl_TiDbl, txt));
    else if (combo == m_ui->coTiAct1 || combo == m_ui->coTiAct2 || combo == m_ui->coTiAct3)
        combo->setCurrentIndex(tbl_txt_lookup(tbl_TiAc, txt));
    else if (combo == m_ui->coTiInAct1 || combo == m_ui->coTiInAct2 || combo == m_ui->coTiInAct3)
        combo->setCurrentIndex(tbl_txt_lookup(tbl_TiInAc, txt));
    else if (combo == m_ui->coTiAct4)
        combo->setCurrentIndex(tbl_txt_lookup(tbl_TiWAc, txt));
    else if (combo == m_ui->leftClickMaximizeButton ||
                combo == m_ui->middleClickMaximizeButton ||
                combo == m_ui->rightClickMaximizeButton) {
        combo->setCurrentIndex(tbl_txt_lookup(tbl_Max, txt));
    } else
        abort();
}

const char* KTitleBarActionsConfig::functionTiDbl(int i)
{
    return tbl_num_lookup(tbl_TiDbl, i);
}

const char* KTitleBarActionsConfig::functionTiAc(int i)
{
    return tbl_num_lookup(tbl_TiAc, i);
}

const char* KTitleBarActionsConfig::functionTiInAc(int i)
{
    return tbl_num_lookup(tbl_TiInAc, i);
}

const char* KTitleBarActionsConfig::functionTiWAc(int i)
{
    return tbl_num_lookup(tbl_TiWAc, i);
}

const char* KTitleBarActionsConfig::functionMax(int i)
{
    return tbl_num_lookup(tbl_Max, i);
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

void KTitleBarActionsConfig::load()
{
    KConfigGroup windowsConfig(config, "Windows");
    setComboText(m_ui->coTiDbl, windowsConfig.readEntry("TitlebarDoubleClickCommand", "Maximize").toAscii());
    setComboText(m_ui->leftClickMaximizeButton, windowsConfig.readEntry(cnf_Max[0], tbl_Max[0]).toAscii());
    setComboText(m_ui->middleClickMaximizeButton, windowsConfig.readEntry(cnf_Max[1], tbl_Max[1]).toAscii());
    setComboText(m_ui->rightClickMaximizeButton, windowsConfig.readEntry(cnf_Max[2], tbl_Max[2]).toAscii());

    KConfigGroup cg(config, "MouseBindings");
    setComboText(m_ui->coTiAct1, cg.readEntry("CommandActiveTitlebar1", "Raise").toAscii());
    setComboText(m_ui->coTiAct2, cg.readEntry("CommandActiveTitlebar2", "Start Window Tab Drag").toAscii());
    setComboText(m_ui->coTiAct3, cg.readEntry("CommandActiveTitlebar3", "Operations menu").toAscii());
    setComboText(m_ui->coTiAct4, cg.readEntry("CommandTitlebarWheel", "Switch to Window Tab to the Left/Right").toAscii());
    setComboText(m_ui->coTiInAct1, cg.readEntry("CommandInactiveTitlebar1", "Activate and raise").toAscii());
    setComboText(m_ui->coTiInAct2, cg.readEntry("CommandInactiveTitlebar2", "Start Window Tab Drag").toAscii());
    setComboText(m_ui->coTiInAct3, cg.readEntry("CommandInactiveTitlebar3", "Operations menu").toAscii());
}

void KTitleBarActionsConfig::save()
{
    KConfigGroup windowsConfig(config, "Windows");
    windowsConfig.writeEntry("TitlebarDoubleClickCommand", functionTiDbl(m_ui->coTiDbl->currentIndex()));
    windowsConfig.writeEntry(cnf_Max[0], functionMax(m_ui->leftClickMaximizeButton->currentIndex()));
    windowsConfig.writeEntry(cnf_Max[1], functionMax(m_ui->middleClickMaximizeButton->currentIndex()));
    windowsConfig.writeEntry(cnf_Max[2], functionMax(m_ui->rightClickMaximizeButton->currentIndex()));

    KConfigGroup cg(config, "MouseBindings");
    cg.writeEntry("CommandActiveTitlebar1", functionTiAc(m_ui->coTiAct1->currentIndex()));
    cg.writeEntry("CommandActiveTitlebar2", functionTiAc(m_ui->coTiAct2->currentIndex()));
    cg.writeEntry("CommandActiveTitlebar3", functionTiAc(m_ui->coTiAct3->currentIndex()));
    cg.writeEntry("CommandInactiveTitlebar1", functionTiInAc(m_ui->coTiInAct1->currentIndex()));
    cg.writeEntry("CommandTitlebarWheel", functionTiWAc(m_ui->coTiAct4->currentIndex()));
    cg.writeEntry("CommandInactiveTitlebar2", functionTiInAc(m_ui->coTiInAct2->currentIndex()));
    cg.writeEntry("CommandInactiveTitlebar3", functionTiInAc(m_ui->coTiInAct3->currentIndex()));

    if (standAlone) {
        config->sync();
        // Send signal to all kwin instances
        QDBusMessage message =
            QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
        QDBusConnection::sessionBus().send(message);

    }
}

void KTitleBarActionsConfig::defaults()
{
    setComboText(m_ui->coTiDbl, "Shade");
    setComboText(m_ui->coTiAct1, "Raise");
    setComboText(m_ui->coTiAct2, "Start Window Tab Drag");
    setComboText(m_ui->coTiAct3, "Operations menu");
    setComboText(m_ui->coTiAct4, "Switch to Window Tab to the Left/Right");
    setComboText(m_ui->coTiInAct1, "Activate and raise");
    setComboText(m_ui->coTiInAct2, "Start Window Tab Drag");
    setComboText(m_ui->coTiInAct3, "Operations menu");
    setComboText(m_ui->leftClickMaximizeButton, tbl_Max[0]);
    setComboText(m_ui->middleClickMaximizeButton, tbl_Max[1]);
    setComboText(m_ui->rightClickMaximizeButton, tbl_Max[2]);
}


KWindowActionsConfig::KWindowActionsConfig(bool _standAlone, KConfig *_config, const KComponentData &inst, QWidget * parent)
    : KCModule(inst, parent), config(_config), standAlone(_standAlone)
    , m_ui(new KWinActionsConfigForm(this))
{
    connect(m_ui->coWin1, SIGNAL(activated(int)), SLOT(changed()));
    connect(m_ui->coWin2, SIGNAL(activated(int)), SLOT(changed()));
    connect(m_ui->coWin3, SIGNAL(activated(int)), SLOT(changed()));
    connect(m_ui->coWinWheel, SIGNAL(activated(int)), SLOT(changed()));
    connect(m_ui->coAllKey, SIGNAL(activated(int)), SLOT(changed()));
    connect(m_ui->coAll1, SIGNAL(activated(int)), SLOT(changed()));
    connect(m_ui->coAll2, SIGNAL(activated(int)), SLOT(changed()));
    connect(m_ui->coAll3, SIGNAL(activated(int)), SLOT(changed()));
    connect(m_ui->coAllW, SIGNAL(activated(int)), SLOT(changed()));
    load();
}

KWindowActionsConfig::~KWindowActionsConfig()
{
    if (standAlone)
        delete config;
}

void KWindowActionsConfig::setComboText(KComboBox* combo, const char*txt)
{
    if (combo == m_ui->coWin1 || combo == m_ui->coWin2 || combo == m_ui->coWin3)
        combo->setCurrentIndex(tbl_txt_lookup(tbl_Win, txt));
    else if (combo == m_ui->coWinWheel)
        combo->setCurrentIndex(tbl_txt_lookup(tbl_WinWheel, txt));
    else if (combo == m_ui->coAllKey)
        combo->setCurrentIndex(tbl_txt_lookup(tbl_AllKey, txt));
    else if (combo == m_ui->coAll1 || combo == m_ui->coAll2 || combo == m_ui->coAll3)
        combo->setCurrentIndex(tbl_txt_lookup(tbl_All, txt));
    else if (combo == m_ui->coAllW)
        combo->setCurrentIndex(tbl_txt_lookup(tbl_AllW, txt));
    else
        abort();
}

const char* KWindowActionsConfig::functionWin(int i)
{
    return tbl_num_lookup(tbl_Win, i);
}

const char* KWindowActionsConfig::functionWinWheel(int i)
{
    return tbl_num_lookup(tbl_WinWheel, i);
}

const char* KWindowActionsConfig::functionAllKey(int i)
{
    return tbl_num_lookup(tbl_AllKey, i);
}

const char* KWindowActionsConfig::functionAll(int i)
{
    return tbl_num_lookup(tbl_All, i);
}

const char* KWindowActionsConfig::functionAllW(int i)
{
    return tbl_num_lookup(tbl_AllW, i);
}

void KWindowActionsConfig::showEvent(QShowEvent *ev)
{
    if (!standAlone) {
        QWidget::showEvent(ev);
        return;
    }
    KCModule::showEvent(ev);
}

void KWindowActionsConfig::load()
{
    KConfigGroup cg(config, "MouseBindings");
    setComboText(m_ui->coWin1, cg.readEntry("CommandWindow1", "Activate, raise and pass click").toAscii());
    setComboText(m_ui->coWin2, cg.readEntry("CommandWindow2", "Activate and pass click").toAscii());
    setComboText(m_ui->coWin3, cg.readEntry("CommandWindow3", "Activate and pass click").toAscii());
    setComboText(m_ui->coWinWheel, cg.readEntry("CommandWindowWheel", "Scroll").toAscii());
    setComboText(m_ui->coAllKey, cg.readEntry("CommandAllKey", "Alt").toAscii());
    setComboText(m_ui->coAll1, cg.readEntry("CommandAll1", "Move").toAscii());
    setComboText(m_ui->coAll2, cg.readEntry("CommandAll2", "Toggle raise and lower").toAscii());
    setComboText(m_ui->coAll3, cg.readEntry("CommandAll3", "Resize").toAscii());
    setComboText(m_ui->coAllW, cg.readEntry("CommandAllWheel", "Nothing").toAscii());
}

void KWindowActionsConfig::save()
{
    KConfigGroup cg(config, "MouseBindings");
    cg.writeEntry("CommandWindow1", functionWin(m_ui->coWin1->currentIndex()));
    cg.writeEntry("CommandWindow2", functionWin(m_ui->coWin2->currentIndex()));
    cg.writeEntry("CommandWindow3", functionWin(m_ui->coWin3->currentIndex()));
    cg.writeEntry("CommandWindowWheel", functionWinWheel(m_ui->coWinWheel->currentIndex()));
    cg.writeEntry("CommandAllKey", functionAllKey(m_ui->coAllKey->currentIndex()));
    cg.writeEntry("CommandAll1", functionAll(m_ui->coAll1->currentIndex()));
    cg.writeEntry("CommandAll2", functionAll(m_ui->coAll2->currentIndex()));
    cg.writeEntry("CommandAll3", functionAll(m_ui->coAll3->currentIndex()));
    cg.writeEntry("CommandAllWheel", functionAllW(m_ui->coAllW->currentIndex()));

    if (standAlone) {
        config->sync();
        // Send signal to all kwin instances
        QDBusMessage message =
            QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
        QDBusConnection::sessionBus().send(message);
    }
}

void KWindowActionsConfig::defaults()
{
    setComboText(m_ui->coWin1, "Activate, raise and pass click");
    setComboText(m_ui->coWin2, "Activate and pass click");
    setComboText(m_ui->coWin3, "Activate and pass click");
    setComboText(m_ui->coWinWheel, "Scroll");
    setComboText(m_ui->coAllKey, "Alt");
    setComboText(m_ui->coAll1, "Move");
    setComboText(m_ui->coAll2, "Toggle raise and lower");
    setComboText(m_ui->coAll3, "Resize");
    setComboText(m_ui->coAllW, "Nothing");
}
