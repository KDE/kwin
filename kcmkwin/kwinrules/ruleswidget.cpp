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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "ruleswidget.h"

#include <klineedit.h>
#include <krestrictedline.h>
#include <kcombobox.h>
#include <qcheckbox.h>
#include <kpushbutton.h>
#include <qlabel.h>
#include <kwinmodule.h>
#include <klocale.h>
#include <qregexp.h>
#include <qwhatsthis.h>
#include <assert.h>
#include <kmessagebox.h>
#include <qtabwidget.h>

#include "../../rules.h"

#include "detectwidget.h"

namespace KWinInternal
{

#define SETUP( var, type ) \
    connect( enable_##var, SIGNAL( toggled( bool )), rule_##var, SLOT( setEnabled( bool ))); \
    connect( enable_##var, SIGNAL( toggled( bool )), this, SLOT( updateEnable##var())); \
    connect( rule_##var, SIGNAL( activated( int )), this, SLOT( updateEnable##var())); \
    QWhatsThis::add( enable_##var, enableDesc ); \
    QWhatsThis::add( rule_##var, type##RuleDesc );

RulesWidget::RulesWidget( QWidget* parent, const char* name )
: RulesWidgetBase( parent, name )
, detect_dlg( NULL )
    {
    QString enableDesc =
        i18n( "Enable this checkbox to alter this window property for the specified window(s)." );
    QString setRuleDesc =
        i18n( "Specify how the window property should be affected:<ul>"
              "<li><em>Do Not Affect:</em> The window property will not be affected and therefore"
              " the default handling for it will be used. Specifying this will block more generic"
              " window settings from taking effect.</li>"
              "<li><em>Apply Initially:</em> The window property will be only set to the given value"
              " after the window is created. No further changes will be affected.</li>"
              "<li><em>Remember:</em> The value of the window property will be remembered and every time"
              " time the window is created, the last remembered value will be applied.</li>"
              "<li><em>Force:</em> The window property will be always forced to the given value.</li></ul>" );
    QString forceRuleDesc =
        i18n( "Specify how the window property should be affected:<ul>"
              "<li><em>Do Not Affect:</em> The window property will not be affected and therefore"
              " the default handling for it will be used. Specifying this will block more generic"
              " window settings from taking effect.</li>"
              "<li><em>Force:</em> The window property will be always forced to the given value.</li></ul>" );
    // window tabs have enable signals done in designer
    // geometry tab
    SETUP( position, set );
    SETUP( size, set );
    SETUP( desktop, set );
    SETUP( maximizehoriz, set );
    SETUP( maximizevert, set );
    SETUP( minimize, set );
    SETUP( shade, set );
    SETUP( fullscreen, set );
    SETUP( placement, force );
    // preferences tab
    SETUP( above, set );
    SETUP( below, set );
    SETUP( noborder, set );
    SETUP( skiptaskbar, set );
    SETUP( skippager, set );
    SETUP( acceptfocus, force );
    SETUP( closeable, force );
    SETUP( opacityactive, force );
    SETUP( opacityinactive, force );
    SETUP( shortcut, force );
    // workarounds tab
    SETUP( fsplevel, force );
    SETUP( moveresizemode, force );
    SETUP( type, force );
    SETUP( ignoreposition, force );
    SETUP( minsize, force );
    SETUP( maxsize, force );
    SETUP( strictgeometry, force );
    KWinModule module;
    int i;
    for( i = 1;
         i <= module.numberOfDesktops();
         ++i )
        desktop->insertItem( QString::number( i ).rightJustify( 2 ) + ":" + module.desktopName( i ));
    desktop->insertItem( i18n( "All Desktops" ));
    }

#undef SETUP

#define UPDATE_ENABLE_SLOT( var ) \
void RulesWidget::updateEnable##var() \
    { \
    /* leave the label readable label_##var->setEnabled( enable_##var->isChecked() && rule_##var->currentItem() != 0 );*/ \
    var->setEnabled( enable_##var->isChecked() && rule_##var->currentItem() != 0 ); \
    }

// geometry tab
UPDATE_ENABLE_SLOT( position )
UPDATE_ENABLE_SLOT( size )
UPDATE_ENABLE_SLOT( desktop )
UPDATE_ENABLE_SLOT( maximizehoriz )
UPDATE_ENABLE_SLOT( maximizevert )
UPDATE_ENABLE_SLOT( minimize )
UPDATE_ENABLE_SLOT( shade )
UPDATE_ENABLE_SLOT( fullscreen )
UPDATE_ENABLE_SLOT( placement )
// preferences tab
UPDATE_ENABLE_SLOT( above )
UPDATE_ENABLE_SLOT( below )
UPDATE_ENABLE_SLOT( noborder )
UPDATE_ENABLE_SLOT( skiptaskbar )
UPDATE_ENABLE_SLOT( skippager )
UPDATE_ENABLE_SLOT( acceptfocus )
UPDATE_ENABLE_SLOT( closeable )
UPDATE_ENABLE_SLOT( opacityactive )
UPDATE_ENABLE_SLOT( opacityinactive )
void RulesWidget::updateEnableshortcut()
    {
    shortcut->setEnabled( enable_shortcut->isChecked() && rule_shortcut->currentItem() != 0 );
    shortcut_edit->setEnabled( enable_shortcut->isChecked() && rule_shortcut->currentItem() != 0 );
    }
// workarounds tab
UPDATE_ENABLE_SLOT( fsplevel )
UPDATE_ENABLE_SLOT( moveresizemode )
UPDATE_ENABLE_SLOT( type )
UPDATE_ENABLE_SLOT( ignoreposition )
UPDATE_ENABLE_SLOT( minsize )
UPDATE_ENABLE_SLOT( maxsize )
UPDATE_ENABLE_SLOT( strictgeometry )

#undef UPDATE_ENABLE_SLOT

static const int set_rule_to_combo[] =
    {
    0, // Unused
    0, // Don't Affect
    3, // Force
    1, // Apply
    2, // Remember
    };

static const Rules::SetRule combo_to_set_rule[] =
    {
    ( Rules::SetRule )Rules::DontAffect,
    ( Rules::SetRule )Rules::Apply,
    ( Rules::SetRule )Rules::Remember,
    ( Rules::SetRule )Rules::Force
    };

static const int force_rule_to_combo[] =
    {
    0, // Unused
    0, // Don't Affect
    1 // Force
    };

static const Rules::ForceRule combo_to_force_rule[] =
    {
    ( Rules::ForceRule )Rules::DontAffect,
    ( Rules::ForceRule )Rules::Force
    };

static QString positionToStr( const QPoint& p )
    {
    if( p == invalidPoint )
        return QString::null;
    return QString::number( p.x()) + "," + QString::number( p.y());
    }

static QPoint strToPosition( const QString& str )
    {            // two numbers, with + or -, separated by any of , x X :
    QRegExp reg( "\\s*([+-]?[0-9]*)\\s*[,xX:]\\s*([+-]?[0-9]*)\\s*" );
    if( !reg.exactMatch( str ))
        return invalidPoint;
    return QPoint( reg.cap( 1 ).toInt(), reg.cap( 2 ).toInt());
    }

static QString sizeToStr( const QSize& s )
    {
    if( !s.isValid())
        return QString::null;
    return QString::number( s.width()) + "," + QString::number( s.height());
    }

static QSize strToSize( const QString& str )
    {            // two numbers, with + or -, separated by any of , x X :
    QRegExp reg( "\\s*([+-]?[0-9]*)\\s*[,xX:]\\s*([+-]?[0-9]*)\\s*" );
    if( !reg.exactMatch( str ))
        return QSize();
    return QSize( reg.cap( 1 ).toInt(), reg.cap( 2 ).toInt());
    }

//used for opacity settings
static QString intToStr( const int& s )
    {
    if( s < 1 || s > 100 )
        return QString::null;
    return QString::number(s);
    }
 
static int strToInt( const QString& str )
    {
    int tmp = str.toInt();
    if( tmp < 1 || tmp > 100 )
        return 100;
    return tmp;
    }    
    
int RulesWidget::desktopToCombo( int d ) const
    {
    if( d >= 1 && d < desktop->count())
        return d - 1;
    return desktop->count() - 1; // on all desktops
    }

int RulesWidget::comboToDesktop( int val ) const
    {
    if( val == desktop->count() - 1 )
        return NET::OnAllDesktops;
    return val + 1;
    }

static int placementToCombo( Placement::Policy placement )
    {
    static const int conv[] = 
        {
        1, // NoPlacement
        0, // Default
        5, // Random
        2, // Smart
        3, // Cascade
        4, // Centered
        6, // ZeroCornered
        7, // UnderMouse
        8 // OnMainWindow
        };
    return conv[ placement ];
    }

static Placement::Policy comboToPlacement( int val )
    {
    static const Placement::Policy conv[] =
        {
        Placement::Default,
        Placement::NoPlacement,
        Placement::Smart,
        Placement::Cascade,
        Placement::Centered,
        Placement::Random,
        Placement::ZeroCornered,
        Placement::UnderMouse,
        Placement::OnMainWindow
        };
    return conv[ val ];
    }

static int moveresizeToCombo( Options::MoveResizeMode mode )
    {
    return mode == Options::Opaque ? 0 : 1;
    }

static Options::MoveResizeMode comboToMoveResize( int val )
    {
    return val == 0 ? Options::Opaque : Options::Transparent;
    }

static int typeToCombo( NET::WindowType type )
    {
    if( type < NET::Normal || type > NET::Splash )
        return 0; // Normal
    static const int conv[] =
        {
        0, // Normal
        7, // Desktop
	3, // Dock
	4, // Toolbar
       	5, // Menu
	1, // Dialog
	8, // Override
        9, // TopMenu
	2, // Utility
	6  // Splash
        };
    return conv[ type ];
    }

static NET::WindowType comboToType( int val )
    {
    static const NET::WindowType conv[] =
        {
        NET::Normal,
        NET::Dialog,
        NET::Utility,
        NET::Dock,
        NET::Toolbar,
        NET::Menu,
        NET::Splash,
        NET::Desktop,
        NET::Override,
        NET::TopMenu
        };
    return conv[ val ];
    }

#define GENERIC_RULE( var, func, Type, type, uimethod, uimethod0 ) \
    if( rules->var##rule == Rules::Unused##Type##Rule ) \
        { \
        enable_##var->setChecked( false ); \
        rule_##var->setCurrentItem( 0 ); \
        var->uimethod0; \
        updateEnable##var(); \
        } \
    else \
        { \
        enable_##var->setChecked( true ); \
        rule_##var->setCurrentItem( type##_rule_to_combo[ rules->var##rule ] ); \
        var->uimethod( func( rules->var )); \
        updateEnable##var(); \
        }

#define CHECKBOX_SET_RULE( var, func ) GENERIC_RULE( var, func, Set, set, setChecked, setChecked( false ))
#define LINEEDIT_SET_RULE( var, func ) GENERIC_RULE( var, func, Set, set, setText, setText( "" ))
#define COMBOBOX_SET_RULE( var, func ) GENERIC_RULE( var, func, Set, set, setCurrentItem, setCurrentItem( 0 ))
#define CHECKBOX_FORCE_RULE( var, func ) GENERIC_RULE( var, func, Force, force, setChecked, setChecked( false ))
#define LINEEDIT_FORCE_RULE( var, func ) GENERIC_RULE( var, func, Force, force, setText, setText( "" ))
#define COMBOBOX_FORCE_RULE( var, func ) GENERIC_RULE( var, func, Force, force, setCurrentItem, setCurrentItem( 0 ))

void RulesWidget::setRules( Rules* rules )
    {
    Rules tmp;
    if( rules == NULL )
        rules = &tmp; // empty
    description->setText( rules->description );
    wmclass->setText( rules->wmclass );
    whole_wmclass->setChecked( rules->wmclasscomplete );
    wmclass_match->setCurrentItem( rules->wmclassmatch );
    wmclassMatchChanged();
    role->setText( rules->windowrole );
    role_match->setCurrentItem( rules->windowrolematch );
    roleMatchChanged();
    types->setSelected( 0, rules->types & NET::NormalMask );
    types->setSelected( 1, rules->types & NET::DialogMask );
    types->setSelected( 2, rules->types & NET::UtilityMask );
    types->setSelected( 3, rules->types & NET::DockMask );
    types->setSelected( 4, rules->types & NET::ToolbarMask );
    types->setSelected( 5, rules->types & NET::MenuMask );
    types->setSelected( 6, rules->types & NET::SplashMask );
    types->setSelected( 7, rules->types & NET::DesktopMask );
    types->setSelected( 8, rules->types & NET::OverrideMask );
    types->setSelected( 9, rules->types & NET::TopMenuMask );
    title->setText( rules->title );
    title_match->setCurrentItem( rules->titlematch );
    titleMatchChanged();
    extra->setText( rules->extrarole );
    extra_match->setCurrentItem( rules->extrarolematch );
    extraMatchChanged();
    machine->setText( rules->clientmachine );
    machine_match->setCurrentItem( rules->clientmachinematch );
    machineMatchChanged();
    LINEEDIT_SET_RULE( position, positionToStr );
    LINEEDIT_SET_RULE( size, sizeToStr );
    COMBOBOX_SET_RULE( desktop, desktopToCombo );
    CHECKBOX_SET_RULE( maximizehoriz, );
    CHECKBOX_SET_RULE( maximizevert, );
    CHECKBOX_SET_RULE( minimize, );
    CHECKBOX_SET_RULE( shade, );
    CHECKBOX_SET_RULE( fullscreen, );
    COMBOBOX_FORCE_RULE( placement, placementToCombo );
    CHECKBOX_SET_RULE( above, );
    CHECKBOX_SET_RULE( below, );
    CHECKBOX_SET_RULE( noborder, );
    CHECKBOX_SET_RULE( skiptaskbar, );
    CHECKBOX_SET_RULE( skippager, );
    CHECKBOX_FORCE_RULE( acceptfocus, );
    CHECKBOX_FORCE_RULE( closeable, );
    LINEEDIT_FORCE_RULE( opacityactive, intToStr );
    LINEEDIT_FORCE_RULE( opacityinactive, intToStr );
    LINEEDIT_SET_RULE( shortcut, );
    COMBOBOX_FORCE_RULE( fsplevel, );
    COMBOBOX_FORCE_RULE( moveresizemode, moveresizeToCombo );
    COMBOBOX_FORCE_RULE( type, typeToCombo );
    CHECKBOX_FORCE_RULE( ignoreposition, );
    LINEEDIT_FORCE_RULE( minsize, sizeToStr );
    LINEEDIT_FORCE_RULE( maxsize, sizeToStr );
    CHECKBOX_FORCE_RULE( strictgeometry, );
    }

#undef GENERIC_RULE
#undef CHECKBOX_SET_RULE
#undef LINEEDIT_SET_RULE
#undef COMBOBOX_SET_RULE
#undef CHECKBOX_FORCE_RULE
#undef LINEEDIT_FORCE_RULE
#undef COMBOBOX_FORCE_RULE

#define GENERIC_RULE( var, func, Type, type, uimethod ) \
    if( enable_##var->isChecked()) \
        { \
        rules->var##rule = combo_to_##type##_rule[ rule_##var->currentItem() ]; \
        rules->var = func( var->uimethod()); \
        } \
    else \
        rules->var##rule = Rules::Unused##Type##Rule;

#define CHECKBOX_SET_RULE( var, func ) GENERIC_RULE( var, func, Set, set, isChecked )
#define LINEEDIT_SET_RULE( var, func ) GENERIC_RULE( var, func, Set, set, text )
#define COMBOBOX_SET_RULE( var, func ) GENERIC_RULE( var, func, Set, set, currentItem )
#define CHECKBOX_FORCE_RULE( var, func ) GENERIC_RULE( var, func, Force, force, isChecked )
#define LINEEDIT_FORCE_RULE( var, func ) GENERIC_RULE( var, func, Force, force, text )
#define COMBOBOX_FORCE_RULE( var, func ) GENERIC_RULE( var, func, Force, force, currentItem )

Rules* RulesWidget::rules() const
    {
    Rules* rules = new Rules();
    rules->description = description->text();
    rules->wmclass = wmclass->text().utf8();
    rules->wmclasscomplete = whole_wmclass->isChecked();
    rules->wmclassmatch = static_cast< Rules::StringMatch >( wmclass_match->currentItem());
    rules->windowrole = role->text().utf8();
    rules->windowrolematch = static_cast< Rules::StringMatch >( role_match->currentItem());
    rules->types = 0;
    bool all_types = true;
    for( unsigned int i = 0;
         i < types->count();
         ++i )
        if( !types->isSelected( i ))
            all_types = false;
    if( all_types ) // if all types are selected, use AllTypesMask (for future expansion)
        rules->types = NET::AllTypesMask;
    else
        {
        rules->types |= types->isSelected( 0 ) ? NET::NormalMask : 0;
        rules->types |= types->isSelected( 1 ) ? NET::DialogMask : 0;
        rules->types |= types->isSelected( 2 ) ? NET::UtilityMask : 0;
        rules->types |= types->isSelected( 3 ) ? NET::DockMask : 0;
        rules->types |= types->isSelected( 4 ) ? NET::ToolbarMask : 0;
        rules->types |= types->isSelected( 5 ) ? NET::MenuMask : 0;
        rules->types |= types->isSelected( 6 ) ? NET::SplashMask : 0;
        rules->types |= types->isSelected( 7 ) ? NET::DesktopMask : 0;
        rules->types |= types->isSelected( 8 ) ? NET::OverrideMask : 0;
        rules->types |= types->isSelected( 9 ) ? NET::TopMenuMask : 0;
        }
    rules->title = title->text();
    rules->titlematch = static_cast< Rules::StringMatch >( title_match->currentItem());
    rules->extrarole = extra->text().utf8();
    rules->extrarolematch = static_cast< Rules::StringMatch >( extra_match->currentItem());
    rules->clientmachine = machine->text().utf8();
    rules->clientmachinematch = static_cast< Rules::StringMatch >( machine_match->currentItem());
    LINEEDIT_SET_RULE( position, strToPosition );
    LINEEDIT_SET_RULE( size, strToSize );
    COMBOBOX_SET_RULE( desktop, comboToDesktop );
    CHECKBOX_SET_RULE( maximizehoriz, );
    CHECKBOX_SET_RULE( maximizevert, );
    CHECKBOX_SET_RULE( minimize, );
    CHECKBOX_SET_RULE( shade, );
    CHECKBOX_SET_RULE( fullscreen, );
    COMBOBOX_FORCE_RULE( placement, comboToPlacement );
    CHECKBOX_SET_RULE( above, );
    CHECKBOX_SET_RULE( below, );
    CHECKBOX_SET_RULE( noborder, );
    CHECKBOX_SET_RULE( skiptaskbar, );
    CHECKBOX_SET_RULE( skippager, );
    CHECKBOX_FORCE_RULE( acceptfocus, );
    CHECKBOX_FORCE_RULE( closeable, );
    LINEEDIT_FORCE_RULE( opacityactive, strToInt );
    LINEEDIT_FORCE_RULE( opacityinactive, strToInt );
    LINEEDIT_SET_RULE( shortcut, );
    COMBOBOX_FORCE_RULE( fsplevel, );
    COMBOBOX_FORCE_RULE( moveresizemode, comboToMoveResize );
    COMBOBOX_FORCE_RULE( type, comboToType );
    CHECKBOX_FORCE_RULE( ignoreposition, );
    LINEEDIT_FORCE_RULE( minsize, strToSize );
    LINEEDIT_FORCE_RULE( maxsize, strToSize );
    CHECKBOX_FORCE_RULE( strictgeometry, );
    return rules;
    }

#undef GENERIC_RULE
#undef CHECKBOX_SET_RULE
#undef LINEEDIT_SET_RULE
#undef COMBOBOX_SET_RULE
#undef CHECKBOX_FORCE_RULE
#undef LINEEDIT_FORCE_RULE
#undef COMBOBOX_FORCE_RULE

#define STRING_MATCH_COMBO( type ) \
void RulesWidget::type##MatchChanged() \
    { \
    edit_reg_##type->setEnabled( type##_match->currentItem() == Rules::RegExpMatch ); \
    type->setEnabled( type##_match->currentItem() != Rules::UnimportantMatch ); \
    }

STRING_MATCH_COMBO( wmclass )
STRING_MATCH_COMBO( role )
STRING_MATCH_COMBO( title )
STRING_MATCH_COMBO( extra )
STRING_MATCH_COMBO( machine )

#undef STRING_MATCH_COMBO

void RulesWidget::detectClicked()
    {
    assert( detect_dlg == NULL );
    detect_dlg = new DetectDialog;
    connect( detect_dlg, SIGNAL( detectionDone( bool )), this, SLOT( detected( bool )));
    detect_dlg->detect( 0 );
    }

void RulesWidget::detected( bool ok )
    {
    if( ok )
        {
        wmclass->setText( detect_dlg->selectedClass());
        wmclass_match->setCurrentItem( Rules::ExactMatch );
        wmclassMatchChanged(); // grrr
        whole_wmclass->setChecked( detect_dlg->selectedWholeClass());
        role->setText( detect_dlg->selectedRole());
        role_match->setCurrentItem( detect_dlg->selectedRole().isEmpty()
            ? Rules::UnimportantMatch : Rules::ExactMatch );
        roleMatchChanged();
        if( detect_dlg->selectedWholeApp())
            {
            for( unsigned int i = 0;
                 i < types->count();
                 ++i )
                types->setSelected( i, true );
            }
        else
            {
            NET::WindowType type = detect_dlg->selectedType();
            for( unsigned int i = 0;
                 i < types->count();
                 ++i )
                types->setSelected( i, false );
            types->setSelected( typeToCombo( type ), true );
            }
        title->setText( detect_dlg->selectedTitle());
        title_match->setCurrentItem( detect_dlg->titleMatch());
        titleMatchChanged();
        machine->setText( detect_dlg->selectedMachine());
        machine_match->setCurrentItem( Rules::UnimportantMatch );
        machineMatchChanged();
        // prefill values from to window to settings which already set
        const KWin::WindowInfo& info = detect_dlg->windowInfo();
        prefillUnusedValues( info );
        }
    delete detect_dlg;
    detect_dlg = NULL;
    detect_dlg_ok = ok;
    }

#define GENERIC_PREFILL( var, func, info, uimethod ) \
    if( !enable_##var->isChecked()) \
        { \
        var->uimethod( func( info )); \
        }

#define CHECKBOX_PREFILL( var, func, info ) GENERIC_PREFILL( var, func, info, setChecked )
#define LINEEDIT_PREFILL( var, func, info ) GENERIC_PREFILL( var, func, info, setText )
#define COMBOBOX_PREFILL( var, func, info ) GENERIC_PREFILL( var, func, info, setCurrentItem )

void RulesWidget::prefillUnusedValues( const KWin::WindowInfo& info )
    {
    LINEEDIT_PREFILL( position, positionToStr, info.frameGeometry().topLeft() );
    LINEEDIT_PREFILL( size, sizeToStr, info.frameGeometry().size() );
    COMBOBOX_PREFILL( desktop, desktopToCombo, info.desktop() );
    CHECKBOX_PREFILL( maximizehoriz,, info.state() & NET::MaxHoriz );
    CHECKBOX_PREFILL( maximizevert,, info.state() & NET::MaxVert );
    CHECKBOX_PREFILL( minimize,, info.isMinimized() );
    CHECKBOX_PREFILL( shade,, info.state() & NET::Shaded );
    CHECKBOX_PREFILL( fullscreen,, info.state() & NET::FullScreen );
    //COMBOBOX_PREFILL( placement, placementToCombo );
    CHECKBOX_PREFILL( above,, info.state() & NET::KeepAbove );
    CHECKBOX_PREFILL( below,, info.state() & NET::KeepBelow );
    // noborder is only internal KWin information, so let's guess
    CHECKBOX_PREFILL( noborder,, info.frameGeometry() == info.geometry() );
    CHECKBOX_PREFILL( skiptaskbar,, info.state() & NET::SkipTaskbar );
    CHECKBOX_PREFILL( skippager,, info.state() & NET::SkipPager );
    //CHECKBOX_PREFILL( acceptfocus, );
    //CHECKBOX_PREFILL( closeable, );
    LINEEDIT_PREFILL( opacityactive, intToStr, 100 /*get the actual opacity somehow*/);
    LINEEDIT_PREFILL( opacityinactive, intToStr, 100 /*get the actual opacity somehow*/);
    //LINEEDIT_PREFILL( shortcut, );
    //COMBOBOX_PREFILL( fsplevel, );
    //COMBOBOX_PREFILL( moveresizemode, moveresizeToCombo );
    COMBOBOX_PREFILL( type, typeToCombo, info.windowType( SUPPORTED_WINDOW_TYPES_MASK ) );
    //CHECKBOX_PREFILL( ignoreposition, );
    LINEEDIT_PREFILL( minsize, sizeToStr, info.frameGeometry().size() );
    LINEEDIT_PREFILL( maxsize, sizeToStr, info.frameGeometry().size() );
    //CHECKBOX_PREFILL( strictgeometry, );
    }

#undef GENERIC_PREFILL
#undef CHECKBOX_PREFILL
#undef LINEEDIT_PREFILL
#undef COMBOBOX_PREFILL

bool RulesWidget::finalCheck()
    {
    if( description->text().isEmpty())
        {
        if( !wmclass->text().isEmpty())
            description->setText( i18n( "Settings for %1" ).arg( wmclass->text()));
        else
            description->setText( i18n( "Unnamed entry" ));
        }
    bool all_types = true;
    for( unsigned int i = 0;
         i < types->count();
         ++i )
        if( !types->isSelected( i ))
            all_types = false;
    if( wmclass_match->currentItem() == Rules::UnimportantMatch && all_types )
        {
        if( KMessageBox::warningContinueCancel( topLevelWidget(),
            i18n( "You have specified the window class as unimportant.\n"
                  "This means the settings will possibly apply to windows from all applications. "
                  "If you really want to create a generic setting, it is recommended you at least "
                  "limit the window types to avoid special window types." )) != KMessageBox::Continue )
            return false;
        }
    return true;
    }

void RulesWidget::prepareWindowSpecific( WId window )
    {
    tabs->setCurrentPage( 2 ); // geometry tab, skip tabs for window identification
    KWin::WindowInfo info( window, -1U, -1U ); // read everything
    prefillUnusedValues( info );
    }

void RulesWidget::shortcutEditClicked()
    {
    EditShortcutDialog dlg( topLevelWidget());
    dlg.setShortcut( shortcut->text());
    if( dlg.exec() == QDialog::Accepted )
        shortcut->setText( dlg.shortcut());
    }

RulesDialog::RulesDialog( QWidget* parent, const char* name )
: KDialogBase( parent, name, true, i18n( "Edit Window-Specific Settings" ), Ok | Cancel )
    {
    widget = new RulesWidget( this );
    setMainWidget( widget );
    }

// window is set only for Alt+F3/Window-specific settings, because the dialog
// is then related to one specific window
Rules* RulesDialog::edit( Rules* r, WId window )
    {
    rules = r;
    widget->setRules( rules );
    if( window != 0 )
        widget->prepareWindowSpecific( window );
    exec();
    return rules;
    }

void RulesDialog::accept()
    {
    if( !widget->finalCheck())
        return;
    rules = widget->rules();
    KDialogBase::accept();
    }

EditShortcut::EditShortcut( QWidget* parent, const char* name )
: EditShortcutBase( parent, name )
    {
    }

void EditShortcut::editShortcut()
    {
    ShortcutDialog dlg( KShortcut( shortcut->text()), topLevelWidget());
    if( dlg.exec() == QDialog::Accepted )
        shortcut->setText( dlg.shortcut().toString());
    }

void EditShortcut::clearShortcut()
    {
    shortcut->setText( "" );
    }

EditShortcutDialog::EditShortcutDialog( QWidget* parent, const char* name )
: KDialogBase( parent, name, true, i18n( "Edit Shortcut" ), Ok | Cancel )
    {
    widget = new EditShortcut( this );
    setMainWidget( widget );
    }

void EditShortcutDialog::setShortcut( const QString& cut )
    {
    widget->shortcut->setText( cut );
    }

QString EditShortcutDialog::shortcut() const
    {
    return widget->shortcut->text();
    }

ShortcutDialog::ShortcutDialog( const KShortcut& cut, QWidget* parent, const char* name )
    : KShortcutDialog( cut, false /*TODO???*/, parent, name )
    {
    }

void ShortcutDialog::accept()
    {
    for( int i = 0;
         ;
         ++i )
        {
        KKeySequence seq = shortcut().seq( i );
        if( seq.isNull())
            break;
        if( seq.key( 0 ) == Key_Escape )
            {
            reject();
            return;
            }
        if( seq.key( 0 ) == Key_Space )
            { // clear
            setShortcut( KShortcut());
            KShortcutDialog::accept();
            return;
            }
        if( seq.key( 0 ).modFlags() == 0 )
            { // no shortcuts without modifiers
            KShortcut cut = shortcut();
            cut.setSeq( i, KKeySequence());
            setShortcut( cut );
            return;
            }
        }
    KShortcutDialog::accept();
    }

} // namespace

#include "ruleswidget.moc"
