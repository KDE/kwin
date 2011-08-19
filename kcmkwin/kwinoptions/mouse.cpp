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

void KTitleBarActionsConfig::paletteChanged()
{
    createMaxButtonPixmaps();
    for (int b = 0; b < 3; ++b)
        for (int t = 0; t < 3; ++t)
            coMax[b]->setItemIcon(t, maxButtonPixmaps[t]);

}

KTitleBarActionsConfig::KTitleBarActionsConfig(bool _standAlone, KConfig *_config, const KComponentData &inst, QWidget * parent)
    : KCModule(inst, parent), config(_config), standAlone(_standAlone)
{
    QString strWin1, strWin2, strWin3, strAllKey, strAll1, strAll2, strAll3;
    QGridLayout *grid;
    QGroupBox *box;
    QLabel *label;
    QString strMouseButton1, strMouseButton3;
    QString txtButton1, txtButton3;
    QStringList items;
    bool leftHandedMouse = (KGlobalSettings::mouseSettings().handed == KGlobalSettings::KMouseSettings::LeftHanded);

    QVBoxLayout *layout = new QVBoxLayout(this);

    /** Titlebar doubleclick ************/

    QWidget *titlebarActions = new QWidget(this);
    QGridLayout *gLayout = new QGridLayout(titlebarActions);
    layout->addWidget(titlebarActions);

    KComboBox* combo = new KComboBox(this);
    combo->addItem(i18nc("@item:inlistbox behavior on double click", "Maximize"));
    combo->addItem(i18n("Maximize (vertical only)"));
    combo->addItem(i18n("Maximize (horizontal only)"));
    combo->addItem(i18nc("@item:inlistbox behavior on double click", "Minimize"));
    combo->addItem(i18n("Shade"));
    combo->addItem(i18n("Lower"));
    combo->addItem(i18nc("@item:inlistbox behavior on double click", "Close"));
    combo->addItem(i18nc("@item:inlistbox behavior on double click", "On All Desktops"));
    combo->addItem(i18n("Nothing"));
    combo->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    connect(combo, SIGNAL(activated(int)), SLOT(changed()));
    coTiDbl = combo;
    combo->setWhatsThis(i18n("Behavior on <em>double</em> click into the titlebar."));

    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    label = new QLabel(i18n("&Titlebar double-click:"), this);
    label->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    label->setBuddy(combo);
    gLayout->addWidget(label, 0, 0);
    gLayout->addWidget(combo, 0, 1);

    /** Mouse Wheel Events  **************/
    // Titlebar and frame mouse Wheel
    KComboBox* comboW = new KComboBox(this);
    comboW->addItem(i18n("Raise/Lower"));
    comboW->addItem(i18n("Shade/Unshade"));
    comboW->addItem(i18n("Maximize/Restore"));
    comboW->addItem(i18n("Keep Above/Below"));
    comboW->addItem(i18n("Move to Previous/Next Desktop"));
    comboW->addItem(i18n("Change Opacity"));
    comboW->addItem(i18n("Switch to Window Tab to the Left/Right"));
    comboW->addItem(i18n("Nothing"));
    comboW->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    connect(comboW, SIGNAL(activated(int)), SLOT(changed()));
    coTiAct4 = comboW;
    comboW->setWhatsThis(i18n("Handle mouse wheel events"));

    comboW->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    label = new QLabel(i18n("Titlebar wheel event:"), this);
    label->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    label->setBuddy(comboW);
    gLayout->addWidget(label, 1, 0);
    gLayout->addWidget(comboW, 1, 1);

    /** Titlebar and frame  **************/

    box = new QGroupBox(i18n("Titlebar && Frame"), this);
    layout->addWidget(box);
    box->setObjectName(QString::fromLatin1("Titlebar and Frame"));
    box->setWhatsThis(i18n("Here you can customize mouse click behavior when clicking on the"
                           " titlebar or the frame of a window."));

    grid = new QGridLayout(box);

    strMouseButton1 = i18n("Left button:");
    txtButton1 = i18n("In this row you can customize left click behavior when clicking into"
                      " the titlebar or the frame.");

    strMouseButton3 = i18n("Right button:");
    txtButton3 = i18n("In this row you can customize right click behavior when clicking into"
                      " the titlebar or the frame.");

    if (leftHandedMouse) {
        qSwap(strMouseButton1, strMouseButton3);
        qSwap(txtButton1, txtButton3);
    }

    label = new QLabel(strMouseButton1, box);
    label->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    grid->addWidget(label, 1, 0);
    label->setWhatsThis(txtButton1);

    label = new QLabel(i18n("Middle button:"), box);
    label->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    grid->addWidget(label, 2, 0);
    label->setWhatsThis(i18n("In this row you can customize middle click behavior when clicking into"
                             " the titlebar or the frame."));

    label = new QLabel(strMouseButton3, box);
    label->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    grid->addWidget(label, 3, 0);
    label->setWhatsThis(txtButton3);


    label = new QLabel(i18n("Active"), box);
    grid->addWidget(label, 0, 1);
    label->setAlignment(Qt::AlignCenter);
    label->setWhatsThis(i18n("In this column you can customize mouse clicks into the titlebar"
                             " or the frame of an active window."));

    // Items for titlebar and frame, active windows
    items << i18n("Raise")
          << i18n("Lower")
          << i18n("Toggle Raise & Lower")
          << i18n("Minimize")
          << i18n("Shade")
          << i18n("Close")
          << i18n("Operations Menu")
          << i18n("Start Window Tab Drag")
          << i18n("Nothing");

    // Titlebar and frame, active, mouse button 1
    combo = new KComboBox(box);
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    grid->addWidget(combo, 1, 1);
    combo->addItems(items);
    connect(combo, SIGNAL(activated(int)), SLOT(changed()));
    coTiAct1 = combo;

    txtButton1 = i18n("Behavior on <em>left</em> click into the titlebar or frame of an "
                      "<em>active</em> window.");

    txtButton3 = i18n("Behavior on <em>right</em> click into the titlebar or frame of an "
                      "<em>active</em> window.");

    // Be nice to left handed users
    if (leftHandedMouse) qSwap(txtButton1, txtButton3);

    combo->setWhatsThis(txtButton1);

    // Titlebar and frame, active, mouse button 2
    combo = new KComboBox(box);
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    grid->addWidget(combo, 2, 1);
    combo->addItems(items);
    connect(combo, SIGNAL(activated(int)), SLOT(changed()));
    coTiAct2 = combo;
    combo->setWhatsThis(i18n("Behavior on <em>middle</em> click into the titlebar or frame of an <em>active</em> window."));

    // Titlebar and frame, active, mouse button 3
    combo = new KComboBox(box);
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    grid->addWidget(combo, 3, 1);
    combo->addItems(items);
    connect(combo, SIGNAL(activated(int)), SLOT(changed()));
    coTiAct3 =  combo;
    combo->setWhatsThis(txtButton3);

    txtButton1 = i18n("Behavior on <em>left</em> click into the titlebar or frame of an "
                      "<em>inactive</em> window.");

    txtButton3 = i18n("Behavior on <em>right</em> click into the titlebar or frame of an "
                      "<em>inactive</em> window.");

    // Be nice to left handed users
    if (leftHandedMouse) qSwap(txtButton1, txtButton3);

    label = new QLabel(i18n("Inactive"), box);
    grid->addWidget(label, 0, 2);
    label->setAlignment(Qt::AlignCenter);
    label->setWhatsThis(i18n("In this column you can customize mouse clicks into the titlebar"
                             " or the frame of an inactive window."));

    // Items for titlebar and frame, inactive windows
    items.clear();
    items  << i18n("Activate & Raise")
           << i18n("Activate & Lower")
           << i18n("Activate")
           << i18n("Raise")
           << i18n("Lower")
           << i18n("Toggle Raise & Lower")
           << i18n("Minimize")
           << i18n("Shade")
           << i18n("Close")
           << i18n("Operations Menu")
           << i18n("Start Window Tab Drag")
           << i18n("Nothing");

    // Titlebar and frame, inactive, mouse button 1
    combo = new KComboBox(box);
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    grid->addWidget(combo, 1, 2);
    combo->addItems(items);
    connect(combo, SIGNAL(activated(int)), SLOT(changed()));
    coTiInAct1 = combo;
    combo->setWhatsThis(txtButton1);

    // Titlebar and frame, inactive, mouse button 2
    combo = new KComboBox(box);
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    grid->addWidget(combo, 2, 2);
    combo->addItems(items);
    connect(combo, SIGNAL(activated(int)), SLOT(changed()));
    coTiInAct2 = combo;
    combo->setWhatsThis(i18n("Behavior on <em>middle</em> click into the titlebar or frame of an <em>inactive</em> window."));

    // Titlebar and frame, inactive, mouse button 3
    combo = new KComboBox(box);
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    grid->addWidget(combo, 3, 2);
    combo->addItems(items);
    connect(combo, SIGNAL(activated(int)), SLOT(changed()));
    coTiInAct3 = combo;
    combo->setWhatsThis(txtButton3);

    /**  Maximize Button ******************/

    box = new QGroupBox(i18n("Maximize Button"), this);
    layout->addWidget(box);
    box->setObjectName(QString::fromLatin1("Maximize Button"));
    box->setWhatsThis(
        i18n("Here you can customize behavior when clicking on the maximize button."));

    QHBoxLayout* hlayout = new QHBoxLayout(box);

    QString strMouseButton[] = {
        i18n("Left button:"),
        i18n("Middle button:"),
        i18n("Right button:")
    };

    QString txtButton[] = {
        i18n("Behavior on <em>left</em> click onto the maximize button."),
        i18n("Behavior on <em>middle</em> click onto the maximize button."),
        i18n("Behavior on <em>right</em> click onto the maximize button.")
    };

    if (leftHandedMouse) {  // Be nice to lefties
        qSwap(strMouseButton[0], strMouseButton[2]);
        qSwap(txtButton[0], txtButton[2]);
    }

    createMaxButtonPixmaps();
    for (int b = 0; b < 3; ++b) {
        if (b != 0) hlayout->addItem(new QSpacerItem(5, 5, QSizePolicy::MinimumExpanding, QSizePolicy::Minimum));  // Spacer

        QLabel * label = new QLabel(strMouseButton[b], box);
        hlayout->addWidget(label);
        label->setWhatsThis(txtButton[b]);
        label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);

        coMax[b] = new ToolTipComboBox(box, tbl_Max);
        hlayout->addWidget(coMax[b]);
        for (int t = 0; t < 3; ++t) coMax[b]->addItem(maxButtonPixmaps[t], QString());
        connect(coMax[b], SIGNAL(activated(int)), SLOT(changed()));
        connect(coMax[b], SIGNAL(activated(int)), coMax[b], SLOT(changed()));
        coMax[b]->setWhatsThis(txtButton[b]);
        coMax[b]->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
    }

    connect(KGlobalSettings::self(), SIGNAL(kdisplayPaletteChanged()), SLOT(paletteChanged()));

    layout->addStretch();

    load();
}

KTitleBarActionsConfig::~KTitleBarActionsConfig()
{
    if (standAlone)
        delete config;
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
    if (combo == coTiDbl)
        combo->setCurrentIndex(tbl_txt_lookup(tbl_TiDbl, txt));
    else if (combo == coTiAct1 || combo == coTiAct2 || combo == coTiAct3)
        combo->setCurrentIndex(tbl_txt_lookup(tbl_TiAc, txt));
    else if (combo == coTiInAct1 || combo == coTiInAct2 || combo == coTiInAct3)
        combo->setCurrentIndex(tbl_txt_lookup(tbl_TiInAc, txt));
    else if (combo == coTiAct4)
        combo->setCurrentIndex(tbl_txt_lookup(tbl_TiWAc, txt));
    else if (combo == coMax[0] || combo == coMax[1] || combo == coMax[2]) {
        combo->setCurrentIndex(tbl_txt_lookup(tbl_Max, txt));
        static_cast<ToolTipComboBox *>(combo)->changed();
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
    setComboText(coTiDbl, windowsConfig.readEntry("TitlebarDoubleClickCommand", "Maximize").toAscii());
    for (int t = 0; t < 3; ++t)
        setComboText(coMax[t], windowsConfig.readEntry(cnf_Max[t], tbl_Max[t]).toAscii());

    KConfigGroup cg(config, "MouseBindings");
    setComboText(coTiAct1, cg.readEntry("CommandActiveTitlebar1", "Raise").toAscii());
    setComboText(coTiAct2, cg.readEntry("CommandActiveTitlebar2", "Start Window Tab Drag").toAscii());
    setComboText(coTiAct3, cg.readEntry("CommandActiveTitlebar3", "Operations menu").toAscii());
    setComboText(coTiAct4, cg.readEntry("CommandTitlebarWheel", "Switch to Window Tab to the Left/Right").toAscii());
    setComboText(coTiInAct1, cg.readEntry("CommandInactiveTitlebar1", "Activate and raise").toAscii());
    setComboText(coTiInAct2, cg.readEntry("CommandInactiveTitlebar2", "Start Window Tab Drag").toAscii());
    setComboText(coTiInAct3, cg.readEntry("CommandInactiveTitlebar3", "Operations menu").toAscii());
}

void KTitleBarActionsConfig::save()
{
    KConfigGroup windowsConfig(config, "Windows");
    windowsConfig.writeEntry("TitlebarDoubleClickCommand", functionTiDbl(coTiDbl->currentIndex()));
    for (int t = 0; t < 3; ++t)
        windowsConfig.writeEntry(cnf_Max[t], functionMax(coMax[t]->currentIndex()));

    KConfigGroup cg(config, "MouseBindings");
    cg.writeEntry("CommandActiveTitlebar1", functionTiAc(coTiAct1->currentIndex()));
    cg.writeEntry("CommandActiveTitlebar2", functionTiAc(coTiAct2->currentIndex()));
    cg.writeEntry("CommandActiveTitlebar3", functionTiAc(coTiAct3->currentIndex()));
    cg.writeEntry("CommandInactiveTitlebar1", functionTiInAc(coTiInAct1->currentIndex()));
    cg.writeEntry("CommandTitlebarWheel", functionTiWAc(coTiAct4->currentIndex()));
    cg.writeEntry("CommandInactiveTitlebar2", functionTiInAc(coTiInAct2->currentIndex()));
    cg.writeEntry("CommandInactiveTitlebar3", functionTiInAc(coTiInAct3->currentIndex()));

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
    setComboText(coTiDbl, "Shade");
    setComboText(coTiAct1, "Raise");
    setComboText(coTiAct2, "Start Window Tab Drag");
    setComboText(coTiAct3, "Operations menu");
    setComboText(coTiAct4, "Switch to Window Tab to the Left/Right");
    setComboText(coTiInAct1, "Activate and raise");
    setComboText(coTiInAct2, "Start Window Tab Drag");
    setComboText(coTiInAct3, "Operations menu");
    for (int t = 0; t < 3; ++t)
        setComboText(coMax[t], tbl_Max[t]);
}


KWindowActionsConfig::KWindowActionsConfig(bool _standAlone, KConfig *_config, const KComponentData &inst, QWidget * parent)
    : KCModule(inst, parent), config(_config), standAlone(_standAlone)
{
    QString strWin1, strWin2, strWin3, strWinWheel, strAllKey, strAll1, strAll2, strAll3, strAllW;
    QGroupBox *box;
    QString strMouseButton1, strMouseButton2, strMouseButton3, strMouseWheel;
    QString txtButton1, txtButton2, txtButton3, txtWheel;
    QStringList items;
    bool leftHandedMouse = (KGlobalSettings::mouseSettings().handed == KGlobalSettings::KMouseSettings::LeftHanded);

    QVBoxLayout *layout = new QVBoxLayout(this);

    /**  Inactive inner window ******************/

    box = new QGroupBox(i18n("Inactive Inner Window"), this);
    layout->addWidget(box);
    box->setObjectName(QString::fromLatin1("Inactive Inner Window"));
    box->setWhatsThis(i18n("Here you can customize mouse click behavior when clicking on an inactive"
                           " inner window ('inner' means: not titlebar, not frame)."));

    QGridLayout *gridLayout = new QGridLayout(box);

    strMouseButton1 = i18n("Left button:");
    txtButton1 = i18n("In this row you can customize left click behavior when clicking into"
                      " the titlebar or the frame.");

    strMouseButton2 = i18n("Middle button:");
    txtButton2 = i18n("In this row you can customize middle click behavior when clicking into"
                      " the titlebar or the frame.");

    strMouseButton3 = i18n("Right button:");
    txtButton3 = i18n("In this row you can customize right click behavior when clicking into"
                      " the titlebar or the frame.");

    strMouseWheel = i18n("Wheel");

    if (leftHandedMouse) {
        qSwap(strMouseButton1, strMouseButton3);
        qSwap(txtButton1, txtButton3);
    }

    strWin1 = i18n("In this row you can customize left click behavior when clicking into"
                   " an inactive inner window ('inner' means: not titlebar, not frame).");

    strWin2 = i18n("In this row you can customize middle click behavior when clicking into"
                   " an inactive inner window ('inner' means: not titlebar, not frame).");

    strWin3 = i18n("In this row you can customize right click behavior when clicking into"
                   " an inactive inner window ('inner' means: not titlebar, not frame).");

    strWinWheel = i18n("In this row you can customize behavior when scrolling into"
                       " an inactive inner window ('inner' means: not titlebar, not frame).");

    // Be nice to lefties
    if (leftHandedMouse) qSwap(strWin1, strWin3);

    items.clear();
    items   << i18n("Activate, Raise & Pass Click")
            << i18n("Activate & Pass Click")
            << i18n("Activate")
            << i18n("Activate & Raise");

    KComboBox* combo = new KComboBox(box);
    coWin1 = combo;
    combo->addItems(items);
    connect(combo, SIGNAL(activated(int)), SLOT(changed()));
    combo->setWhatsThis(strWin1);
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QLabel* label = new QLabel(strMouseButton1, this);
    label->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    label->setBuddy(combo);
    gridLayout->addWidget(label, 0, 0);
    gridLayout->addWidget(combo, 0, 1);

    combo = new KComboBox(box);
    combo->addItems(items);
    connect(combo, SIGNAL(activated(int)), SLOT(changed()));
    coWin2 = combo;
    combo->setWhatsThis(strWin2);
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    label = new QLabel(strMouseButton2, this);
    label->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    label->setBuddy(combo);
    gridLayout->addWidget(label, 1, 0);
    gridLayout->addWidget(combo, 1, 1);

    combo = new KComboBox(box);
    combo->addItems(items);
    connect(combo, SIGNAL(activated(int)), SLOT(changed()));
    coWin3 = combo;
    combo->setWhatsThis(strWin3);
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    label = new QLabel(strMouseButton3, this);
    label->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    label->setBuddy(combo);
    gridLayout->addWidget(label, 2, 0);
    gridLayout->addWidget(combo, 2, 1);

    items.clear();
    items   << i18n("Scroll")
            << i18n("Activate & Scroll")
            << i18n("Activate, Raise & Scroll");

    combo = new KComboBox(box);
    combo->addItems(items);
    connect(combo, SIGNAL(activated(int)), SLOT(changed()));
    coWinWheel = combo;
    combo->setWhatsThis(strWinWheel);
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    label = new QLabel(strMouseWheel, this);
    label->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    label->setBuddy(combo);
    gridLayout->addWidget(label, 3, 0);
    gridLayout->addWidget(combo, 3, 1);




    /** Inner window, titlebar and frame **************/

    box = new QGroupBox(i18n("Inner Window, Titlebar && Frame"), this);
    layout->addWidget(box);
    box->setObjectName(QString::fromLatin1("Inner Window, Titlebar and Frame"));
    box->setWhatsThis(i18n("Here you can customize KDE's behavior when clicking somewhere into"
                           " a window while pressing a modifier key."));

    QHBoxLayout* innerLay = new QHBoxLayout(box);
    QHBoxLayout* fLay = new QHBoxLayout;
    gridLayout = new QGridLayout;
    innerLay->addLayout(fLay);
    innerLay->addLayout(gridLayout);

    // Labels
    strMouseButton1 = i18n("Left button:");
    strAll1 = i18n("In this row you can customize left click behavior when clicking into"
                   " the titlebar or the frame.");

    strMouseButton2 = i18n("Middle button:");
    strAll2 = i18n("In this row you can customize middle click behavior when clicking into"
                   " the titlebar or the frame.");

    strMouseButton3 = i18n("Right button:");
    strAll3 = i18n("In this row you can customize right click behavior when clicking into"
                   " the titlebar or the frame.");

    if (leftHandedMouse) {
        qSwap(strMouseButton1, strMouseButton3);
        qSwap(strAll1, strAll3);
    }

    // Combo's
    combo = new KComboBox(box);
    combo->addItem(i18n("Meta"));
    combo->addItem(i18n("Alt"));
    connect(combo, SIGNAL(activated(int)), SLOT(changed()));
    coAllKey = combo;
    combo->setWhatsThis(i18n("Here you select whether holding the Meta key or Alt key "
                             "will allow you to perform the following actions."));
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    label = new QLabel(i18n("Modifier key:"), this);
    label->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    label->setBuddy(combo);
    fLay->addWidget(label);
    fLay->addWidget(combo);
    fLay->addWidget(new QLabel("   + ", this));


    items.clear();
    items << i18n("Move")
          << i18n("Activate, Raise and Move")
          << i18n("Toggle Raise & Lower")
          << i18n("Resize")
          << i18n("Raise")
          << i18n("Lower")
          << i18n("Minimize")
          << i18n("Decrease Opacity")
          << i18n("Increase Opacity")
          << i18n("Nothing");

    combo = new KComboBox(box);
    combo->addItems(items);
    connect(combo, SIGNAL(activated(int)), SLOT(changed()));
    coAll1 = combo;
    combo->setWhatsThis(strAll1);
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    label = new QLabel(strMouseButton1, this);
    label->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    label->setBuddy(combo);
    gridLayout->addWidget(label, 0, 0);
    gridLayout->addWidget(combo, 0, 1);


    combo = new KComboBox(box);
    combo->addItems(items);
    connect(combo, SIGNAL(activated(int)), SLOT(changed()));
    coAll2 = combo;
    combo->setWhatsThis(strAll2);
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    label = new QLabel(strMouseButton2, this);
    label->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    label->setBuddy(combo);
    gridLayout->addWidget(label, 1, 0);
    gridLayout->addWidget(combo, 1, 1);

    combo = new KComboBox(box);
    combo->addItems(items);
    connect(combo, SIGNAL(activated(int)), SLOT(changed()));
    coAll3 =  combo;
    combo->setWhatsThis(strAll3);
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    label = new QLabel(strMouseButton3, this);
    label->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    label->setBuddy(combo);
    gridLayout->addWidget(label, 2, 0);
    gridLayout->addWidget(combo, 2, 1);


    combo = new KComboBox(box);
    combo->addItem(i18n("Raise/Lower"));
    combo->addItem(i18n("Shade/Unshade"));
    combo->addItem(i18n("Maximize/Restore"));
    combo->addItem(i18n("Keep Above/Below"));
    combo->addItem(i18n("Move to Previous/Next Desktop"));
    combo->addItem(i18n("Change Opacity"));
    combo->addItem(i18n("Switch to Window Tab to the Left/Right"));
    combo->addItem(i18n("Nothing"));
    connect(combo, SIGNAL(activated(int)), SLOT(changed()));
    coAllW =  combo;
    combo->setWhatsThis(i18n("Here you can customize KDE's behavior when scrolling with the mouse wheel"
                             " in a window while pressing the modifier key."));
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    label = new QLabel(i18n("Mouse wheel:"), this);
    label->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    label->setBuddy(combo);
    gridLayout->addWidget(label, 3, 0);
    gridLayout->addWidget(combo, 3, 1);


    layout->addStretch();
    load();
}

KWindowActionsConfig::~KWindowActionsConfig()
{
    if (standAlone)
        delete config;
}

void KWindowActionsConfig::setComboText(KComboBox* combo, const char*txt)
{
    if (combo == coWin1 || combo == coWin2 || combo == coWin3)
        combo->setCurrentIndex(tbl_txt_lookup(tbl_Win, txt));
    else if (combo == coWinWheel)
        combo->setCurrentIndex(tbl_txt_lookup(tbl_WinWheel, txt));
    else if (combo == coAllKey)
        combo->setCurrentIndex(tbl_txt_lookup(tbl_AllKey, txt));
    else if (combo == coAll1 || combo == coAll2 || combo == coAll3)
        combo->setCurrentIndex(tbl_txt_lookup(tbl_All, txt));
    else if (combo == coAllW)
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
    setComboText(coWin1, cg.readEntry("CommandWindow1", "Activate, raise and pass click").toAscii());
    setComboText(coWin2, cg.readEntry("CommandWindow2", "Activate and pass click").toAscii());
    setComboText(coWin3, cg.readEntry("CommandWindow3", "Activate and pass click").toAscii());
    setComboText(coWinWheel, cg.readEntry("CommandWindowWheel", "Scroll").toAscii());
    setComboText(coAllKey, cg.readEntry("CommandAllKey", "Alt").toAscii());
    setComboText(coAll1, cg.readEntry("CommandAll1", "Move").toAscii());
    setComboText(coAll2, cg.readEntry("CommandAll2", "Toggle raise and lower").toAscii());
    setComboText(coAll3, cg.readEntry("CommandAll3", "Resize").toAscii());
    setComboText(coAllW, cg.readEntry("CommandAllWheel", "Nothing").toAscii());
}

void KWindowActionsConfig::save()
{
    KConfigGroup cg(config, "MouseBindings");
    cg.writeEntry("CommandWindow1", functionWin(coWin1->currentIndex()));
    cg.writeEntry("CommandWindow2", functionWin(coWin2->currentIndex()));
    cg.writeEntry("CommandWindow3", functionWin(coWin3->currentIndex()));
    cg.writeEntry("CommandWindowWheel", functionWinWheel(coWinWheel->currentIndex()));
    cg.writeEntry("CommandAllKey", functionAllKey(coAllKey->currentIndex()));
    cg.writeEntry("CommandAll1", functionAll(coAll1->currentIndex()));
    cg.writeEntry("CommandAll2", functionAll(coAll2->currentIndex()));
    cg.writeEntry("CommandAll3", functionAll(coAll3->currentIndex()));
    cg.writeEntry("CommandAllWheel", functionAllW(coAllW->currentIndex()));

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
    setComboText(coWin1, "Activate, raise and pass click");
    setComboText(coWin2, "Activate and pass click");
    setComboText(coWin3, "Activate and pass click");
    setComboText(coWinWheel, "Scroll");
    setComboText(coAllKey, "Alt");
    setComboText(coAll1, "Move");
    setComboText(coAll2, "Toggle raise and lower");
    setComboText(coAll3, "Resize");
    setComboText(coAllW, "Nothing");
}
