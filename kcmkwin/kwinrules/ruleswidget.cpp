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

#include "ruleswidget.h"

#include <klineedit.h>
#include <kcombobox.h>
#include <kcolorschememanager.h>
#include <QCheckBox>
#include <QFileInfo>
#include <QLabel>
#include <kwindowsystem.h>
#include <KLocalizedString>
#include <QRegExp>

#ifdef KWIN_BUILD_ACTIVITIES
#include <kactivities/consumer.h>
#endif

#include <assert.h>
#include <kmessagebox.h>
#include <QTabWidget>
#include <QTimer>

#include "../../rules.h"

#include "detectwidget.h"

Q_DECLARE_METATYPE(NET::WindowType)

namespace KWin
{

#define SETUP( var, type ) \
    connect( enable_##var, SIGNAL(toggled(bool)), rule_##var, SLOT(setEnabled(bool))); \
    connect( enable_##var, SIGNAL(toggled(bool)), this, SLOT(updateEnable##var())); \
    connect( rule_##var, SIGNAL(activated(int)), this, SLOT(updateEnable##var())); \
    enable_##var->setWhatsThis( enableDesc ); \
    rule_##var->setWhatsThis( type##RuleDesc );

RulesWidget::RulesWidget(QWidget* parent)
    : detect_dlg(nullptr)
{
    Q_UNUSED(parent);
    setupUi(this);
    QRegularExpressionValidator* validator = new QRegularExpressionValidator(QRegularExpression("[0-9\\-+,xX:]*"), this);
    maxsize->setValidator(validator);
    minsize->setValidator(validator);
    position->setValidator(validator);
    Ui::RulesWidgetBase::size->setValidator(validator);

    QString enableDesc =
        i18n("Enable this checkbox to alter this window property for the specified window(s).");
    QString setRuleDesc =
        i18n("Specify how the window property should be affected:<ul>"
             "<li><em>Do Not Affect:</em> The window property will not be affected and therefore"
             " the default handling for it will be used. Specifying this will block more generic"
             " window settings from taking effect.</li>"
             "<li><em>Apply Initially:</em> The window property will be only set to the given value"
             " after the window is created. No further changes will be affected.</li>"
             "<li><em>Remember:</em> The value of the window property will be remembered and every"
             " time the window is created, the last remembered value will be applied.</li>"
             "<li><em>Force:</em> The window property will be always forced to the given value.</li>"
             "<li><em>Apply Now:</em> The window property will be set to the given value immediately"
             " and will not be affected later (this action will be deleted afterwards).</li>"
             "<li><em>Force temporarily:</em> The window property will be forced to the given value"
             " until it is hidden (this action will be deleted after the window is hidden).</li>"
             "</ul>");
    QString forceRuleDesc =
        i18n("Specify how the window property should be affected:<ul>"
             "<li><em>Do Not Affect:</em> The window property will not be affected and therefore"
             " the default handling for it will be used. Specifying this will block more generic"
             " window settings from taking effect.</li>"
             "<li><em>Force:</em> The window property will be always forced to the given value.</li>"
             "<li><em>Force temporarily:</em> The window property will be forced to the given value"
             " until it is hidden (this action will be deleted after the window is hidden).</li>"
             "</ul>");
    // window tabs have enable signals done in designer
    // geometry tab
    SETUP(position, set);
    SETUP(size, set);
    SETUP(desktop, set);
    SETUP(screen, set);
#ifdef KWIN_BUILD_ACTIVITIES
    SETUP(activity, set);
#endif
    SETUP(maximizehoriz, set);
    SETUP(maximizevert, set);
    SETUP(minimize, set);
    SETUP(shade, set);
    SETUP(fullscreen, set);
    SETUP(placement, force);
    // preferences tab
    SETUP(above, set);
    SETUP(below, set);
    SETUP(noborder, set);
    SETUP(decocolor, force);
    SETUP(skiptaskbar, set);
    SETUP(skippager, set);
    SETUP(skipswitcher, set);
    SETUP(acceptfocus, force);
    SETUP(closeable, force);
    SETUP(autogroup, force);
    SETUP(autogroupfg, force);
    SETUP(autogroupid, force);
    SETUP(opacityactive, force);
    SETUP(opacityinactive, force);
    SETUP(shortcut, force);
    // workarounds tab
    SETUP(fsplevel, force);
    SETUP(fpplevel, force);
    SETUP(type, force);
    SETUP(desktopfile, set);
    SETUP(ignoregeometry, set);
    SETUP(minsize, force);
    SETUP(maxsize, force);
    SETUP(strictgeometry, force);
    SETUP(disableglobalshortcuts, force);
    SETUP(blockcompositing, force);

    connect (shortcut_edit, SIGNAL(clicked()), SLOT(shortcutEditClicked()));

    edit_reg_wmclass->hide();
    edit_reg_role->hide();
    edit_reg_title->hide();
    edit_reg_machine->hide();

#ifndef KWIN_BUILD_ACTIVITIES
    rule_activity->hide();
    enable_activity->hide();
    activity->hide();
#endif
    int i;
    for (i = 1;
            i <= KWindowSystem::numberOfDesktops();
            ++i)
        desktop->addItem(QString::number(i).rightJustified(2) + ':' + KWindowSystem::desktopName(i));
    desktop->addItem(i18n("All Desktops"));

#ifdef KWIN_BUILD_ACTIVITIES
    m_activities = new KActivities::Consumer(this);
    connect(m_activities, &KActivities::Consumer::activitiesChanged,
            this, [this] { updateActivitiesList(); });
    connect(m_activities, &KActivities::Consumer::serviceStatusChanged,
            this, [this] { updateActivitiesList(); });
    updateActivitiesList();
#endif

    KColorSchemeManager *schemes = new KColorSchemeManager(this);
    decocolor->setModel(schemes->model());

    // hide autogrouping as it's currently not supported
    // BUG 370301
    line_11->hide();
    enable_autogroup->hide();
    autogroup->hide();
    rule_autogroup->hide();
    enable_autogroupid->hide();
    autogroupid->hide();
    rule_autogroupid->hide();
    enable_autogroupfg->hide();
    autogroupfg->hide();
    rule_autogroupfg->hide();
}

#undef SETUP

#define UPDATE_ENABLE_SLOT(var) \
    void RulesWidget::updateEnable##var() \
    { \
        /* leave the label readable label_##var->setEnabled( enable_##var->isChecked() && rule_##var->currentIndex() != 0 );*/ \
        Ui::RulesWidgetBase::var->setEnabled( enable_##var->isChecked() && rule_##var->currentIndex() != 0 ); \
    }

// geometry tab
UPDATE_ENABLE_SLOT(position)
UPDATE_ENABLE_SLOT(size)
UPDATE_ENABLE_SLOT(desktop)
UPDATE_ENABLE_SLOT(screen)
#ifdef KWIN_BUILD_ACTIVITIES
UPDATE_ENABLE_SLOT(activity)
#endif
UPDATE_ENABLE_SLOT(maximizehoriz)
UPDATE_ENABLE_SLOT(maximizevert)
UPDATE_ENABLE_SLOT(minimize)
UPDATE_ENABLE_SLOT(shade)
UPDATE_ENABLE_SLOT(fullscreen)
UPDATE_ENABLE_SLOT(placement)
// preferences tab
UPDATE_ENABLE_SLOT(above)
UPDATE_ENABLE_SLOT(below)
UPDATE_ENABLE_SLOT(noborder)
UPDATE_ENABLE_SLOT(decocolor)
UPDATE_ENABLE_SLOT(skiptaskbar)
UPDATE_ENABLE_SLOT(skippager)
UPDATE_ENABLE_SLOT(skipswitcher)
UPDATE_ENABLE_SLOT(acceptfocus)
UPDATE_ENABLE_SLOT(closeable)
UPDATE_ENABLE_SLOT(autogroup)
UPDATE_ENABLE_SLOT(autogroupfg)
UPDATE_ENABLE_SLOT(autogroupid)
UPDATE_ENABLE_SLOT(opacityactive)
UPDATE_ENABLE_SLOT(opacityinactive)
void RulesWidget::updateEnableshortcut()
{
    shortcut->setEnabled(enable_shortcut->isChecked() && rule_shortcut->currentIndex() != 0);
    shortcut_edit->setEnabled(enable_shortcut->isChecked() && rule_shortcut->currentIndex() != 0);
}
// workarounds tab
UPDATE_ENABLE_SLOT(fsplevel)
UPDATE_ENABLE_SLOT(fpplevel)
UPDATE_ENABLE_SLOT(type)
UPDATE_ENABLE_SLOT(ignoregeometry)
UPDATE_ENABLE_SLOT(minsize)
UPDATE_ENABLE_SLOT(maxsize)
UPDATE_ENABLE_SLOT(strictgeometry)
UPDATE_ENABLE_SLOT(disableglobalshortcuts)
UPDATE_ENABLE_SLOT(blockcompositing)
UPDATE_ENABLE_SLOT(desktopfile)

#undef UPDATE_ENABLE_SLOT

static const int set_rule_to_combo[] = {
    0, // Unused
    0, // Don't Affect
    3, // Force
    1, // Apply
    2, // Remember
    4, // ApplyNow
    5  // ForceTemporarily
};

static const Rules::SetRule combo_to_set_rule[] = {
    (Rules::SetRule)Rules::DontAffect,
    (Rules::SetRule)Rules::Apply,
    (Rules::SetRule)Rules::Remember,
    (Rules::SetRule)Rules::Force,
    (Rules::SetRule)Rules::ApplyNow,
    (Rules::SetRule)Rules::ForceTemporarily
};

static const int force_rule_to_combo[] = {
    0, // Unused
    0, // Don't Affect
    1, // Force
    0, // Apply
    0, // Remember
    0, // ApplyNow
    2  // ForceTemporarily
};

static const Rules::ForceRule combo_to_force_rule[] = {
    (Rules::ForceRule)Rules::DontAffect,
    (Rules::ForceRule)Rules::Force,
    (Rules::ForceRule)Rules::ForceTemporarily
};

static QString positionToStr(const QPoint& p)
{
    if (p == invalidPoint)
        return QString();
    return QString::number(p.x()) + ',' + QString::number(p.y());
}

static QPoint strToPosition(const QString& str)
{
    // two numbers, with + or -, separated by any of , x X :
    QRegExp reg("\\s*([+-]?[0-9]*)\\s*[,xX:]\\s*([+-]?[0-9]*)\\s*");
    if (!reg.exactMatch(str))
        return invalidPoint;
    return QPoint(reg.cap(1).toInt(), reg.cap(2).toInt());
}

static QString sizeToStr(const QSize& s)
{
    if (!s.isValid())
        return QString();
    return QString::number(s.width()) + ',' + QString::number(s.height());
}

static QSize strToSize(const QString& str)
{
    // two numbers, with + or -, separated by any of , x X :
    QRegExp reg("\\s*([+-]?[0-9]*)\\s*[,xX:]\\s*([+-]?[0-9]*)\\s*");
    if (!reg.exactMatch(str))
        return QSize();
    return QSize(reg.cap(1).toInt(), reg.cap(2).toInt());
}

int RulesWidget::desktopToCombo(int d) const
{
    if (d >= 1 && d < desktop->count())
        return d - 1;
    return desktop->count() - 1; // on all desktops
}

int RulesWidget::comboToDesktop(int val) const
{
    if (val == desktop->count() - 1)
        return NET::OnAllDesktops;
    return val + 1;
}

#ifdef KWIN_BUILD_ACTIVITIES
int RulesWidget::activityToCombo(QString d) const
{
    // TODO: ivan - do a multiselection list
    for (int i = 0; i < activity->count(); i++) {
        if (activity->itemData(i).toString() == d) {
            return i;
        }
    }

    return activity->count() - 1; // on all activities
}

QString RulesWidget::comboToActivity(int val) const
{
    // TODO: ivan - do a multiselection list
    if (val < 0 || val >= activity->count())
        return QString();

    return activity->itemData(val).toString();
}

void RulesWidget::updateActivitiesList()
{
    activity->clear();

    // cloned from kactivities/src/lib/core/consumer.cpp
    #define NULL_UUID "00000000-0000-0000-0000-000000000000"
    activity->addItem(i18n("All Activities"), QString::fromLatin1(NULL_UUID));
    #undef NULL_UUID

    if (m_activities->serviceStatus() == KActivities::Consumer::Running) {
        foreach (const QString & activityId, m_activities->activities(KActivities::Info::Running)) {
            const KActivities::Info info(activityId);
            activity->addItem(info.name(), activityId);
        }
    }

    auto rules = this->rules();
    if (rules->activityrule == Rules::UnusedSetRule) {
        enable_activity->setChecked(false);
        Ui::RulesWidgetBase::activity->setCurrentIndex(0);
    } else {
        enable_activity->setChecked(true);
        Ui::RulesWidgetBase::activity->setCurrentIndex(activityToCombo(m_selectedActivityId));
    }
    updateEnableactivity();
}
#endif

static int placementToCombo(Placement::Policy placement)
{
    static const int conv[] = {
        1, // NoPlacement
        0, // Default
        0, // Unknown
        6, // Random
        2, // Smart
        4, // Cascade
        5, // Centered
        7, // ZeroCornered
        8, // UnderMouse
        9, // OnMainWindow
        3  // Maximizing
    };
    return conv[ placement ];
}

static Placement::Policy comboToPlacement(int val)
{
    static const Placement::Policy conv[] = {
        Placement::Default,
        Placement::NoPlacement,
        Placement::Smart,
        Placement::Maximizing,
        Placement::Cascade,
        Placement::Centered,
        Placement::Random,
        Placement::ZeroCornered,
        Placement::UnderMouse,
        Placement::OnMainWindow
        // no Placement::Unknown
    };
    return conv[ val ];
}

static int typeToCombo(NET::WindowType type)
{
    if (type < NET::Normal || type > NET::Splash ||
        type == NET::Override) // The user must NOT set a window to be unmanaged.
                               // This case is not handled in KWin and will lead to segfaults.
                               // Even iff it was supported, it would mean to allow the user to shoot himself
                               // since an unmanaged window has to manage itself, what is probably not the case when the hint is not set.
                               // Rule opportunity might be a relict from the Motif Hint window times of KDE1
        return 0; // Normal
    static const int conv[] = {
        0, // Normal
        7, // Desktop
        3, // Dock
        4, // Toolbar
        5, // Menu
        1, // Dialog
        8, // Override - ignored.
        9, // TopMenu
        2, // Utility
        6  // Splash
    };
    return conv[ type ];
}

static NET::WindowType comboToType(int val)
{
    static const NET::WindowType conv[] = {
        NET::Normal,
        NET::Dialog,
        NET::Utility,
        NET::Dock,
        NET::Toolbar,
        NET::Menu,
        NET::Splash,
        NET::Desktop,
        NET::TopMenu
    };
    return conv[ val ];
}

#define GENERIC_RULE( var, func, Type, type, uimethod, uimethod0 ) \
    if ( rules->var##rule == Rules::Unused##Type##Rule ) \
    { \
        enable_##var->setChecked( false ); \
        rule_##var->setCurrentIndex( 0 ); \
        Ui::RulesWidgetBase::var->uimethod0;    \
        updateEnable##var(); \
    } \
    else \
    { \
        enable_##var->setChecked( true ); \
        rule_##var->setCurrentIndex( type##_rule_to_combo[ rules->var##rule ] ); \
        Ui::RulesWidgetBase::var->uimethod( func( rules->var )); \
        updateEnable##var(); \
    }

#define CHECKBOX_SET_RULE( var, func ) GENERIC_RULE( var, func, Set, set, setChecked, setChecked( false ))
#define LINEEDIT_SET_RULE( var, func ) GENERIC_RULE( var, func, Set, set, setText, setText( QString() ))
#define COMBOBOX_SET_RULE( var, func ) GENERIC_RULE( var, func, Set, set, setCurrentIndex, setCurrentIndex( 0 ))
#define SPINBOX_SET_RULE( var, func ) GENERIC_RULE( var, func, Set, set, setValue, setValue(0))
#define CHECKBOX_FORCE_RULE( var, func ) GENERIC_RULE( var, func, Force, force, setChecked, setChecked( false ))
#define LINEEDIT_FORCE_RULE( var, func ) GENERIC_RULE( var, func, Force, force, setText, setText( QString() ))
#define COMBOBOX_FORCE_RULE( var, func ) GENERIC_RULE( var, func, Force, force, setCurrentIndex, setCurrentIndex( 0 ))
#define SPINBOX_FORCE_RULE( var, func ) GENERIC_RULE( var, func, Force, force, setValue, setValue(0))

void RulesWidget::setRules(Rules* rules)
{
    Rules tmp;
    if (rules == nullptr)
        rules = &tmp; // empty
    description->setText(rules->description);
    wmclass->setText(rules->wmclass);
    whole_wmclass->setChecked(rules->wmclasscomplete);
    wmclass_match->setCurrentIndex(rules->wmclassmatch);
    wmclassMatchChanged();
    role->setText(rules->windowrole);
    role_match->setCurrentIndex(rules->windowrolematch);
    roleMatchChanged();
    types->item(0)->setSelected(rules->types & NET::NormalMask);
    types->item(1)->setSelected(rules->types & NET::DialogMask);
    types->item(2)->setSelected(rules->types & NET::UtilityMask);
    types->item(3)->setSelected(rules->types & NET::DockMask);
    types->item(4)->setSelected(rules->types & NET::ToolbarMask);
    types->item(5)->setSelected(rules->types & NET::MenuMask);
    types->item(6)->setSelected(rules->types & NET::SplashMask);
    types->item(7)->setSelected(rules->types & NET::DesktopMask);
    types->item(8)->setSelected(rules->types & NET::OverrideMask);
    types->item(9)->setSelected(rules->types & NET::TopMenuMask);
    title->setText(rules->title);
    title_match->setCurrentIndex(rules->titlematch);
    titleMatchChanged();
    machine->setText(rules->clientmachine);
    machine_match->setCurrentIndex(rules->clientmachinematch);
    machineMatchChanged();
    LINEEDIT_SET_RULE(position, positionToStr);
    LINEEDIT_SET_RULE(size, sizeToStr);
    COMBOBOX_SET_RULE(desktop, desktopToCombo);
    SPINBOX_SET_RULE(screen, inc);
#ifdef KWIN_BUILD_ACTIVITIES
    m_selectedActivityId = rules->activity;
    COMBOBOX_SET_RULE(activity, activityToCombo);
#endif
    CHECKBOX_SET_RULE(maximizehoriz,);
    CHECKBOX_SET_RULE(maximizevert,);
    CHECKBOX_SET_RULE(minimize,);
    CHECKBOX_SET_RULE(shade,);
    CHECKBOX_SET_RULE(fullscreen,);
    COMBOBOX_FORCE_RULE(placement, placementToCombo);
    CHECKBOX_SET_RULE(above,);
    CHECKBOX_SET_RULE(below,);
    CHECKBOX_SET_RULE(noborder,);
    auto decoColorToCombo = [this](const QString &value) {
        for (int i = 0; i < decocolor->count(); ++i) {
            if (decocolor->itemData(i).toString() == value) {
                return i;
            }
        }
        // search for Breeze
        for (int i = 0; i < decocolor->count(); ++i) {
            if (QFileInfo(decocolor->itemData(i).toString()).baseName() == QStringLiteral("Breeze")) {
                return i;
            }
        }
        return 0;
    };
    COMBOBOX_FORCE_RULE(decocolor, decoColorToCombo);
    CHECKBOX_SET_RULE(skiptaskbar,);
    CHECKBOX_SET_RULE(skippager,);
    CHECKBOX_SET_RULE(skipswitcher,);
    CHECKBOX_FORCE_RULE(acceptfocus,);
    CHECKBOX_FORCE_RULE(closeable,);
    CHECKBOX_FORCE_RULE(autogroup,);
    CHECKBOX_FORCE_RULE(autogroupfg,);
    LINEEDIT_FORCE_RULE(autogroupid,);
    SPINBOX_FORCE_RULE(opacityactive,);
    SPINBOX_FORCE_RULE(opacityinactive,);
    LINEEDIT_SET_RULE(shortcut,);
    COMBOBOX_FORCE_RULE(fsplevel,);
    COMBOBOX_FORCE_RULE(fpplevel,);
    COMBOBOX_FORCE_RULE(type, typeToCombo);
    CHECKBOX_SET_RULE(ignoregeometry,);
    LINEEDIT_FORCE_RULE(minsize, sizeToStr);
    LINEEDIT_FORCE_RULE(maxsize, sizeToStr);
    CHECKBOX_FORCE_RULE(strictgeometry,);
    CHECKBOX_FORCE_RULE(disableglobalshortcuts,);
    CHECKBOX_FORCE_RULE(blockcompositing,);
    LINEEDIT_SET_RULE(desktopfile,)
}

#undef GENERIC_RULE
#undef CHECKBOX_SET_RULE
#undef LINEEDIT_SET_RULE
#undef COMBOBOX_SET_RULE
#undef SPINBOX_SET_RULE
#undef CHECKBOX_FORCE_RULE
#undef LINEEDIT_FORCE_RULE
#undef COMBOBOX_FORCE_RULE
#undef SPINBOX_FORCE_RULE

#define GENERIC_RULE( var, func, Type, type, uimethod ) \
    if ( enable_##var->isChecked() && rule_##var->currentIndex() >= 0) \
    { \
        rules->var##rule = combo_to_##type##_rule[ rule_##var->currentIndex() ]; \
        rules->var = func( Ui::RulesWidgetBase::var->uimethod()); \
    } \
    else \
        rules->var##rule = Rules::Unused##Type##Rule;

#define CHECKBOX_SET_RULE( var, func ) GENERIC_RULE( var, func, Set, set, isChecked )
#define LINEEDIT_SET_RULE( var, func ) GENERIC_RULE( var, func, Set, set, text )
#define COMBOBOX_SET_RULE( var, func ) GENERIC_RULE( var, func, Set, set, currentIndex )
#define SPINBOX_SET_RULE( var, func ) GENERIC_RULE( var, func, Set, set, value)
#define CHECKBOX_FORCE_RULE( var, func ) GENERIC_RULE( var, func, Force, force, isChecked )
#define LINEEDIT_FORCE_RULE( var, func ) GENERIC_RULE( var, func, Force, force, text )
#define COMBOBOX_FORCE_RULE( var, func ) GENERIC_RULE( var, func, Force, force, currentIndex )
#define SPINBOX_FORCE_RULE( var, func ) GENERIC_RULE( var, func, Force, force, value)

Rules* RulesWidget::rules() const
{
    Rules* rules = new Rules();
    rules->description = description->text();
    rules->wmclass = wmclass->text().toUtf8();
    rules->wmclasscomplete = whole_wmclass->isChecked();
    rules->wmclassmatch = static_cast< Rules::StringMatch >(wmclass_match->currentIndex());
    rules->windowrole = role->text().toUtf8();
    rules->windowrolematch = static_cast< Rules::StringMatch >(role_match->currentIndex());
    rules->types = 0;
    bool all_types = true;
    for (int i = 0;
            i < types->count();
            ++i)
        if (!types->item(i)->isSelected())
            all_types = false;
    if (all_types)   // if all types are selected, use AllTypesMask (for future expansion)
        rules->types = NET::AllTypesMask;
    else {
        rules->types |= types->item(0)->isSelected() ? NET::NormalMask   : NET::WindowTypeMask(0);
        rules->types |= types->item(1)->isSelected() ? NET::DialogMask   : NET::WindowTypeMask(0);
        rules->types |= types->item(2)->isSelected() ? NET::UtilityMask  : NET::WindowTypeMask(0);
        rules->types |= types->item(3)->isSelected() ? NET::DockMask     : NET::WindowTypeMask(0);
        rules->types |= types->item(4)->isSelected() ? NET::ToolbarMask  : NET::WindowTypeMask(0);
        rules->types |= types->item(5)->isSelected() ? NET::MenuMask     : NET::WindowTypeMask(0);
        rules->types |= types->item(6)->isSelected() ? NET::SplashMask   : NET::WindowTypeMask(0);
        rules->types |= types->item(7)->isSelected() ? NET::DesktopMask  : NET::WindowTypeMask(0);
        rules->types |= types->item(8)->isSelected() ? NET::OverrideMask : NET::WindowTypeMask(0);
        rules->types |= types->item(9)->isSelected() ? NET::TopMenuMask  : NET::WindowTypeMask(0);
    }
    rules->title = title->text();
    rules->titlematch = static_cast< Rules::StringMatch >(title_match->currentIndex());
    rules->clientmachine = machine->text().toUtf8();
    rules->clientmachinematch = static_cast< Rules::StringMatch >(machine_match->currentIndex());
    LINEEDIT_SET_RULE(position, strToPosition);
    LINEEDIT_SET_RULE(size, strToSize);
    COMBOBOX_SET_RULE(desktop, comboToDesktop);
    SPINBOX_SET_RULE(screen, dec);
#ifdef KWIN_BUILD_ACTIVITIES
    COMBOBOX_SET_RULE(activity, comboToActivity);
#endif
    CHECKBOX_SET_RULE(maximizehoriz,);
    CHECKBOX_SET_RULE(maximizevert,);
    CHECKBOX_SET_RULE(minimize,);
    CHECKBOX_SET_RULE(shade,);
    CHECKBOX_SET_RULE(fullscreen,);
    COMBOBOX_FORCE_RULE(placement, comboToPlacement);
    CHECKBOX_SET_RULE(above,);
    CHECKBOX_SET_RULE(below,);
    CHECKBOX_SET_RULE(noborder,);
    auto comboToDecocolor = [this](int index) -> QString {
        return QFileInfo(decocolor->itemData(index).toString()).baseName();
    };
    COMBOBOX_FORCE_RULE(decocolor, comboToDecocolor);
    CHECKBOX_SET_RULE(skiptaskbar,);
    CHECKBOX_SET_RULE(skippager,);
    CHECKBOX_SET_RULE(skipswitcher,);
    CHECKBOX_FORCE_RULE(acceptfocus,);
    CHECKBOX_FORCE_RULE(closeable,);
    CHECKBOX_FORCE_RULE(autogroup,);
    CHECKBOX_FORCE_RULE(autogroupfg,);
    LINEEDIT_FORCE_RULE(autogroupid,);
    SPINBOX_FORCE_RULE(opacityactive,);
    SPINBOX_FORCE_RULE(opacityinactive,);
    LINEEDIT_SET_RULE(shortcut,);
    COMBOBOX_FORCE_RULE(fsplevel,);
    COMBOBOX_FORCE_RULE(fpplevel,);
    COMBOBOX_FORCE_RULE(type, comboToType);
    CHECKBOX_SET_RULE(ignoregeometry,);
    LINEEDIT_FORCE_RULE(minsize, strToSize);
    LINEEDIT_FORCE_RULE(maxsize, strToSize);
    CHECKBOX_FORCE_RULE(strictgeometry,);
    CHECKBOX_FORCE_RULE(disableglobalshortcuts,);
    CHECKBOX_FORCE_RULE(blockcompositing,);
    LINEEDIT_SET_RULE(desktopfile,);
    return rules;
}

#undef GENERIC_RULE
#undef CHECKBOX_SET_RULE
#undef LINEEDIT_SET_RULE
#undef COMBOBOX_SET_RULE
#undef SPINBOX_SET_RULE
#undef CHECKBOX_FORCE_RULE
#undef LINEEDIT_FORCE_RULE
#undef COMBOBOX_FORCE_RULE
#undef SPINBOX_FORCE_RULE

#define STRING_MATCH_COMBO( type ) \
    void RulesWidget::type##MatchChanged() \
    { \
        edit_reg_##type->setEnabled( type##_match->currentIndex() == Rules::RegExpMatch ); \
        type->setEnabled( type##_match->currentIndex() != Rules::UnimportantMatch ); \
    }

STRING_MATCH_COMBO(wmclass)
STRING_MATCH_COMBO(role)
STRING_MATCH_COMBO(title)
STRING_MATCH_COMBO(machine)

#undef STRING_MATCH_COMBO

void RulesWidget::detectClicked()
{
    assert(detect_dlg == nullptr);
    detect_dlg = new DetectDialog;
    connect(detect_dlg, SIGNAL(detectionDone(bool)), this, SLOT(detected(bool)));
    detect_dlg->detect(Ui::RulesWidgetBase::detection_delay->value());
}

void RulesWidget::detected(bool ok)
{
    if (ok) {
        wmclass->setText(detect_dlg->selectedClass());
        wmclass_match->setCurrentIndex(Rules::ExactMatch);
        wmclassMatchChanged(); // grrr
        whole_wmclass->setChecked(detect_dlg->selectedWholeClass());
        role->setText(detect_dlg->selectedRole());
        role_match->setCurrentIndex(detect_dlg->selectedRole().isEmpty()
                                    ? Rules::UnimportantMatch : Rules::ExactMatch);
        roleMatchChanged();
        if (detect_dlg->selectedWholeApp()) {
            for (int i = 0;
                    i < types->count();
                    ++i)
                types->item(i)->setSelected(true);
        } else {
            NET::WindowType type = detect_dlg->selectedType();
            for (int i = 0;
                    i < types->count();
                    ++i)
                types->item(i)->setSelected(false);
            types->item(typeToCombo(type))->setSelected(true);
        }
        title->setText(detect_dlg->selectedTitle());
        title_match->setCurrentIndex(detect_dlg->titleMatch());
        titleMatchChanged();
        machine->setText(detect_dlg->selectedMachine());
        machine_match->setCurrentIndex(Rules::UnimportantMatch);
        machineMatchChanged();
        // prefill values from to window to settings which already set
        prefillUnusedValues(detect_dlg->windowInfo());
    }
    delete detect_dlg;
    detect_dlg = nullptr;
    detect_dlg_ok = ok;
}

#define GENERIC_PREFILL( var, func, info, uimethod ) \
    if ( !enable_##var->isChecked()) \
    { \
        Ui::RulesWidgetBase::var->uimethod( func( info )); \
    }

#define CHECKBOX_PREFILL( var, func, info ) GENERIC_PREFILL( var, func, info, setChecked )
#define LINEEDIT_PREFILL( var, func, info ) GENERIC_PREFILL( var, func, info, setText )
#define COMBOBOX_PREFILL( var, func, info ) GENERIC_PREFILL( var, func, info, setCurrentIndex )
#define SPINBOX_PREFILL( var, func, info ) GENERIC_PREFILL( var, func, info, setValue )

void RulesWidget::prefillUnusedValues(const KWindowInfo& info)
{
    LINEEDIT_PREFILL(position, positionToStr, info.frameGeometry().topLeft());
    LINEEDIT_PREFILL(size, sizeToStr, info.frameGeometry().size());
    COMBOBOX_PREFILL(desktop, desktopToCombo, info.desktop());
    // COMBOBOX_PREFILL(activity, activityToCombo, info.activity()); // TODO: ivan
    CHECKBOX_PREFILL(maximizehoriz, , info.state() & NET::MaxHoriz);
    CHECKBOX_PREFILL(maximizevert, , info.state() & NET::MaxVert);
    CHECKBOX_PREFILL(minimize, , info.isMinimized());
    CHECKBOX_PREFILL(shade, , info.state() & NET::Shaded);
    CHECKBOX_PREFILL(fullscreen, , info.state() & NET::FullScreen);
    //COMBOBOX_PREFILL( placement, placementToCombo );
    CHECKBOX_PREFILL(above, , info.state() & NET::KeepAbove);
    CHECKBOX_PREFILL(below, , info.state() & NET::KeepBelow);
    // noborder is only internal KWin information, so let's guess
    CHECKBOX_PREFILL(noborder, , info.frameGeometry() == info.geometry());
    CHECKBOX_PREFILL(skiptaskbar, , info.state() & NET::SkipTaskbar);
    CHECKBOX_PREFILL(skippager, , info.state() & NET::SkipPager);
    CHECKBOX_PREFILL(skipswitcher, , info.state() & NET::SkipSwitcher);
    //CHECKBOX_PREFILL( acceptfocus, );
    //CHECKBOX_PREFILL( closeable, );
    //CHECKBOX_PREFILL( autogroup, );
    //CHECKBOX_PREFILL( autogroupfg, );
    //LINEEDIT_PREFILL( autogroupid, );
    SPINBOX_PREFILL(opacityactive, , 100 /*get the actual opacity somehow*/);
    SPINBOX_PREFILL(opacityinactive, , 100 /*get the actual opacity somehow*/);
    //LINEEDIT_PREFILL( shortcut, );
    //COMBOBOX_PREFILL( fsplevel, );
    //COMBOBOX_PREFILL( fpplevel, );
    COMBOBOX_PREFILL(type, typeToCombo, info.windowType(SUPPORTED_MANAGED_WINDOW_TYPES_MASK));
    //CHECKBOX_PREFILL( ignoregeometry, );
    LINEEDIT_PREFILL(minsize, sizeToStr, info.frameGeometry().size());
    LINEEDIT_PREFILL(maxsize, sizeToStr, info.frameGeometry().size());
    //CHECKBOX_PREFILL( strictgeometry, );
    //CHECKBOX_PREFILL( disableglobalshortcuts, );
    //CHECKBOX_PREFILL( blockcompositing, );
    LINEEDIT_PREFILL(desktopfile, , info.desktopFileName());
}

void RulesWidget::prefillUnusedValues(const QVariantMap& info)
{
    const QSize windowSize{info.value("width").toInt(), info.value("height").toInt()};
    LINEEDIT_PREFILL(position, positionToStr, QPoint(info.value("x").toInt(), info.value("y").toInt()));
    LINEEDIT_PREFILL(size, sizeToStr, windowSize);
    COMBOBOX_PREFILL(desktop, desktopToCombo, info.value("x11DesktopNumber").toInt());
    // COMBOBOX_PREFILL(activity, activityToCombo, info.activity()); // TODO: ivan
    CHECKBOX_PREFILL(maximizehoriz, , info.value("maximizeHorizontal").toBool());
    CHECKBOX_PREFILL(maximizevert, , info.value("maximizeVertical").toBool());
    CHECKBOX_PREFILL(minimize, , info.value("minimized").toBool());
    CHECKBOX_PREFILL(shade, , info.value("shaded").toBool());
    CHECKBOX_PREFILL(fullscreen, , info.value("fullscreen").toBool());
    //COMBOBOX_PREFILL( placement, placementToCombo );
    CHECKBOX_PREFILL(above, , info.value("keepAbove").toBool());
    CHECKBOX_PREFILL(below, , info.value("keepBelow").toBool());
    CHECKBOX_PREFILL(noborder, , info.value("noBorder").toBool());
    CHECKBOX_PREFILL(skiptaskbar, , info.value("skipTaskbar").toBool());
    CHECKBOX_PREFILL(skippager, , info.value("skipPager").toBool());
    CHECKBOX_PREFILL(skipswitcher, , info.value("skipSwitcher").toBool());
    //CHECKBOX_PREFILL( acceptfocus, );
    //CHECKBOX_PREFILL( closeable, );
    //CHECKBOX_PREFILL( autogroup, );
    //CHECKBOX_PREFILL( autogroupfg, );
    //LINEEDIT_PREFILL( autogroupid, );
    SPINBOX_PREFILL(opacityactive, , 100 /*get the actual opacity somehow*/);
    SPINBOX_PREFILL(opacityinactive, , 100 /*get the actual opacity somehow*/);
    //LINEEDIT_PREFILL( shortcut, );
    //COMBOBOX_PREFILL( fsplevel, );
    //COMBOBOX_PREFILL( fpplevel, );
    COMBOBOX_PREFILL(type, typeToCombo, info.value("type").value<NET::WindowType>());
    //CHECKBOX_PREFILL( ignoregeometry, );
    LINEEDIT_PREFILL(minsize, sizeToStr, windowSize);
    LINEEDIT_PREFILL(maxsize, sizeToStr, windowSize);
    //CHECKBOX_PREFILL( strictgeometry, );
    //CHECKBOX_PREFILL( disableglobalshortcuts, );
    //CHECKBOX_PREFILL( blockcompositing, );
    LINEEDIT_PREFILL(desktopfile, , info.value("desktopFile").toString());
}

#undef GENERIC_PREFILL
#undef CHECKBOX_PREFILL
#undef LINEEDIT_PREFILL
#undef COMBOBOX_PREFILL
#undef SPINBOX_PREFILL

bool RulesWidget::finalCheck()
{
    if (description->text().isEmpty()) {
        if (!wmclass->text().isEmpty())
            description->setText(i18n("Settings for %1", wmclass->text()));
        else
            description->setText(i18n("Unnamed entry"));
    }
    bool all_types = true;
    for (int i = 0;
            i < types->count();
            ++i)
        if (!types->item(i)->isSelected())
            all_types = false;
    if (wmclass_match->currentIndex() == Rules::UnimportantMatch && all_types) {
        if (KMessageBox::warningContinueCancel(window(),
                                              i18n("You have specified the window class as unimportant.\n"
                                                      "This means the settings will possibly apply to windows from all applications. "
                                                      "If you really want to create a generic setting, it is recommended you at least "
                                                      "limit the window types to avoid special window types.")) != KMessageBox::Continue)
            return false;
    }
    return true;
}

void RulesWidget::prepareWindowSpecific(WId window)
{
    // TODO: adjust for Wayland
    tabs->setCurrentIndex(1);   // geometry tab, skip tab for window identification
    KWindowInfo info(window, NET::WMAllProperties, NET::WM2AllProperties);   // read everything
    prefillUnusedValues(info);
}

void RulesWidget::shortcutEditClicked()
{
    QPointer<EditShortcutDialog> dlg = new EditShortcutDialog(window());
    dlg->setShortcut(shortcut->text());
    if (dlg->exec() == QDialog::Accepted)
        shortcut->setText(dlg->shortcut());
    delete dlg;
}

RulesDialog::RulesDialog(QWidget* parent, const char* name)
    : QDialog(parent)
{
    setObjectName(name);
    setModal(true);
    setWindowTitle(i18n("Edit Window-Specific Settings"));
    setWindowIcon(QIcon::fromTheme("preferences-system-windows-actions"));

    setLayout(new QVBoxLayout);
    widget = new RulesWidget(this);
    layout()->addWidget(widget);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, SIGNAL(accepted()), SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), SLOT(reject()));
    layout()->addWidget(buttons);
}

// window is set only for Alt+F3/Window-specific settings, because the dialog
// is then related to one specific window
Rules* RulesDialog::edit(Rules* r, WId window, bool show_hints)
{
    rules = r;
    widget->setRules(rules);
    if (window != 0)
        widget->prepareWindowSpecific(window);
    if (show_hints)
        QTimer::singleShot(0, this, SLOT(displayHints()));
    exec();
    return rules;
}

void RulesDialog::displayHints()
{
    QString str = "<qt><p>";
    str += i18n("This configuration dialog allows altering settings only for the selected window"
                " or application. Find the setting you want to affect, enable the setting using the checkbox,"
                " select in what way the setting should be affected and to which value.");
#if 0 // maybe later
    str += "</p><p>" + i18n("Consult the documentation for more details.");
#endif
    str += "</p></qt>";
    KMessageBox::information(this, str, QString(), "displayhints");
}

void RulesDialog::accept()
{
    if (!widget->finalCheck())
        return;
    rules = widget->rules();
    QDialog::accept();
}

EditShortcut::EditShortcut(QWidget* parent)
    : QWidget(parent)
{
    setupUi(this);
}

void EditShortcut::editShortcut()
{
    QPointer< ShortcutDialog > dlg = new ShortcutDialog(QKeySequence(shortcut->text()), window());
    if (dlg->exec() == QDialog::Accepted)
        shortcut->setText(dlg->shortcut().toString());
    delete dlg;
}

void EditShortcut::clearShortcut()
{
    shortcut->clear();
}

EditShortcutDialog::EditShortcutDialog(QWidget* parent, const char* name)
    : QDialog(parent)
    , widget(new EditShortcut(this))
{
    setObjectName(name);
    setModal(true);
    setWindowTitle(i18n("Edit Shortcut"));

    setLayout(new QVBoxLayout);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, SIGNAL(accepted()), SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), SLOT(reject()));

    layout()->addWidget(widget);
    layout()->addWidget(buttons);
}

void EditShortcutDialog::setShortcut(const QString& cut)
{
    widget->shortcut->setText(cut);
}

QString EditShortcutDialog::shortcut() const
{
    return widget->shortcut->text();
}

ShortcutDialog::ShortcutDialog(const QKeySequence& cut, QWidget* parent)
    : QDialog(parent)
    , widget(new KKeySequenceWidget(this))
{
    widget->setKeySequence(cut);
    // It's a global shortcut so don't allow multikey shortcuts
    widget->setMultiKeyShortcutsAllowed(false);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, SIGNAL(accepted()), SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), SLOT(reject()));

    setLayout(new QVBoxLayout);
    layout()->addWidget(widget);
    layout()->addWidget(buttons);
}

void ShortcutDialog::accept()
{
    QKeySequence seq = shortcut();
    if (!seq.isEmpty()) {
        if (seq[0] == Qt::Key_Escape) {
            reject();
            return;
        }
        if (seq[0] == Qt::Key_Space
                || (seq[0] & Qt::KeyboardModifierMask) == 0) {
            // clear
            widget->clearKeySequence();
            QDialog::accept();
            return;
        }
    }
    QDialog::accept();
}

QKeySequence ShortcutDialog::shortcut() const
{
    return widget->keySequence();
}

} // namespace

