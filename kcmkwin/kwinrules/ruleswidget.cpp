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

#include "../../rules.h"

namespace KWinInternal
{

#define SETUP_ENABLE( var ) \
    connect( enable_##var, SIGNAL( toggled( bool )), rule_##var, SLOT( setEnabled( bool ))); \
    connect( enable_##var, SIGNAL( toggled( bool )), this, SLOT( updateEnable##var())); \
    connect( rule_##var, SIGNAL( activated( int )), this, SLOT( updateEnable##var()));

RulesWidget::RulesWidget( QWidget* parent, const char* name )
: RulesWidgetBase( parent, name )
    {
    // window tabs have enable signals done in designer
    // geometry tab
    SETUP_ENABLE( position );
    SETUP_ENABLE( size );
    SETUP_ENABLE( desktop );
    SETUP_ENABLE( maximizehoriz );
    SETUP_ENABLE( maximizevert );
    SETUP_ENABLE( minimize );
    SETUP_ENABLE( shade );
    SETUP_ENABLE( fullscreen );
    SETUP_ENABLE( placement );
    // preferences tab
    SETUP_ENABLE( above );
    SETUP_ENABLE( below );
    SETUP_ENABLE( noborder );
    SETUP_ENABLE( skiptaskbar );
    SETUP_ENABLE( skippager );
    SETUP_ENABLE( acceptfocus );
    SETUP_ENABLE( closeable );
    // workarounds tab
    SETUP_ENABLE( fsplevel );
    SETUP_ENABLE( moveresizemode );
    SETUP_ENABLE( type );
    SETUP_ENABLE( ignoreposition );
    SETUP_ENABLE( minsize );
    SETUP_ENABLE( maxsize );
    KWinModule module;
    int i;
    for( i = 1;
         i <= module.numberOfDesktops();
         ++i )
        desktop->insertItem( QString::number( i ).rightJustify( 2 ) + ":" + module.desktopName( i ));
    for(;
         i <= 16;
         ++i )
        desktop->insertItem( QString::number( i ).rightJustify( 2 ));
    desktop->insertItem( i18n( "All Desktops" ));
    }

#undef ENABLE_SETUP

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
// workarounds tab
UPDATE_ENABLE_SLOT( fsplevel )
UPDATE_ENABLE_SLOT( moveresizemode )
UPDATE_ENABLE_SLOT( type )
UPDATE_ENABLE_SLOT( ignoreposition )
UPDATE_ENABLE_SLOT( minsize )
UPDATE_ENABLE_SLOT( maxsize )

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
    return QString::number( p.x()) + "," + QString::number( p.y());
    }

static QPoint strToPosition( const QString& str )
    {            // two numbers, with + or -, separated by any of , x X :
    QRegExp reg( "\\s*[+-]?[0-9]*\\s*[,xX:]\\s*[-+]?[0-9]*\\s*" );
    if( !reg.exactMatch( str ))
        return invalidPoint;
    return QPoint( reg.cap( 1 ).toInt(), reg.cap( 2 ).toInt());
    }

static QString sizeToStr( const QSize& s )
    {
    return QString::number( s.width()) + "," + QString::number( s.height());
    }

static QSize strToSize( const QString& str )
    {            // two numbers, with + or -, separated by any of , x X :
    QRegExp reg( "\\s*[+-]?[0-9]*\\s*[,xX:]\\s*[-+]?[0-9]*\\s*" );
    if( !reg.exactMatch( str ))
        return QSize();
    return QSize( reg.cap( 1 ).toInt(), reg.cap( 2 ).toInt());
    }


static int desktopToCombo( int desktop )
    {
    if( desktop >= 1 && desktop <= 16 )
        return desktop - 1;
    return 16; // on all desktops
    }

static int comboToDesktop( int val )
    {
    if( val == 16 )
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
    reg_wmclass->setChecked( rules->wmclassregexp );
    role->setText( rules->windowrole );
    reg_role->setChecked( rules->windowroleregexp );
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
    reg_title->setChecked( rules->titleregexp );
    extra->setText( rules->extrarole );
    reg_extra->setChecked( rules->extraroleregexp );
    machine->setText( rules->clientmachine );
    reg_machine->setChecked( rules->clientmachineregexp );
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
    COMBOBOX_FORCE_RULE( fsplevel, );
    COMBOBOX_FORCE_RULE( moveresizemode, moveresizeToCombo );
    COMBOBOX_FORCE_RULE( type, typeToCombo );
    CHECKBOX_FORCE_RULE( ignoreposition, );
    LINEEDIT_FORCE_RULE( minsize, sizeToStr );
    LINEEDIT_FORCE_RULE( maxsize, sizeToStr );
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
    rules->wmclassregexp = reg_wmclass->isChecked();
    rules->windowrole = role->text().utf8();
    rules->windowroleregexp = reg_role->isChecked();
    rules->types = 0;
    bool all_types = true;
    for( int i = 0;
         i <= 9;
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
    rules->titleregexp = reg_title->isChecked();
    rules->extrarole = extra->text().utf8();
    rules->extraroleregexp = reg_extra->isChecked();
    rules->clientmachine = machine->text().utf8();
    rules->clientmachineregexp = reg_machine->isChecked();
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
    COMBOBOX_FORCE_RULE( fsplevel, );
    COMBOBOX_FORCE_RULE( moveresizemode, comboToMoveResize );
    COMBOBOX_FORCE_RULE( type, comboToType );
    CHECKBOX_FORCE_RULE( ignoreposition, );
    LINEEDIT_FORCE_RULE( minsize, strToSize );
    LINEEDIT_FORCE_RULE( maxsize, strToSize );
    return rules;
    }

#undef GENERIC_RULE
#undef CHECKBOX_SET_RULE
#undef LINEEDIT_SET_RULE
#undef COMBOBOX_SET_RULE
#undef CHECKBOX_FORCE_RULE
#undef LINEEDIT_FORCE_RULE
#undef COMBOBOX_FORCE_RULE

RulesDialog::RulesDialog( QWidget* parent, const char* name )
: KDialogBase( parent, name, true, "", Ok | Cancel )
    {
    widget = new RulesWidget( this );
    setMainWidget( widget );
    }

Rules* RulesDialog::edit( Rules* r )
    {
    rules = r;
    widget->setRules( rules );
    exec();
    return rules;
    }

void RulesDialog::accept()
    {
    KDialogBase::accept();
    rules = widget->rules();
    }

} // namespace

#include "ruleswidget.moc"
