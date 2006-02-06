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

#include "ruleslist.h"

#include <klistbox.h>
#include <kpushbutton.h>
#include <assert.h>
#include <kdebug.h>
#include <kconfig.h>

#include "ruleswidget.h"

namespace KWinInternal
{

KCMRulesList::KCMRulesList( QWidget* parent, const char* name )
: KCMRulesListBase( parent, name )
    {
    // connect both current/selected, so that current==selected (stupid QListBox :( )
    connect( rules_listbox, SIGNAL( currentChanged( Q3ListBoxItem* )),
        SLOT( activeChanged( Q3ListBoxItem*)));
    connect( rules_listbox, SIGNAL( selectionChanged( Q3ListBoxItem* )),
        SLOT( activeChanged( Q3ListBoxItem*)));
    connect( new_button, SIGNAL( clicked()),
        SLOT( newClicked()));
    connect( modify_button, SIGNAL( clicked()),
        SLOT( modifyClicked()));
    connect( delete_button, SIGNAL( clicked()),
        SLOT( deleteClicked()));
    connect( moveup_button, SIGNAL( clicked()),
        SLOT( moveupClicked()));
    connect( movedown_button, SIGNAL( clicked()),
        SLOT( movedownClicked()));
    connect( rules_listbox, SIGNAL( doubleClicked ( Q3ListBoxItem * ) ),
            SLOT( modifyClicked()));
    load();
    }

KCMRulesList::~KCMRulesList()
    {
    for( QVector< Rules* >::Iterator it = rules.begin();
         it != rules.end();
         ++it )
        delete *it;
    rules.clear();
    }

void KCMRulesList::activeChanged( Q3ListBoxItem* item )
    {
    if( item != NULL )
        rules_listbox->setSelected( item, true ); // make current==selected
    modify_button->setEnabled( item != NULL );
    delete_button->setEnabled( item != NULL );
    moveup_button->setEnabled( item != NULL && item->prev() != NULL );
    movedown_button->setEnabled( item != NULL && item->next() != NULL );
    }

void KCMRulesList::newClicked()
    {
    RulesDialog dlg;
    Rules* rule = dlg.edit( NULL, 0, false );
    if( rule == NULL )
        return;
    int pos = rules_listbox->currentItem() + 1;
    rules_listbox->insertItem( rule->description, pos );
    rules_listbox->setSelected( pos, true );
    rules.insert( rules.begin() + pos, rule );
    emit changed( true );
    }

void KCMRulesList::modifyClicked()
    {
    int pos = rules_listbox->currentItem();
    if ( pos == -1 )
        return;
    RulesDialog dlg;
    Rules* rule = dlg.edit( rules[ pos ], 0, false );
    if( rule == rules[ pos ] )
        return;
    delete rules[ pos ];
    rules[ pos ] = rule;
    rules_listbox->changeItem( rule->description, pos );
    emit changed( true );
    }

void KCMRulesList::deleteClicked()
    {
    int pos = rules_listbox->currentItem();
    assert( pos != -1 );
    rules_listbox->removeItem( pos );
    rules.erase( rules.begin() + pos );
    emit changed( true );
    }

void KCMRulesList::moveupClicked()
    {
    int pos = rules_listbox->currentItem();
    assert( pos != -1 );
    if( pos > 0 )
        {
        QString txt = rules_listbox->text( pos );
        rules_listbox->removeItem( pos );
        rules_listbox->insertItem( txt, pos - 1 );
        rules_listbox->setSelected( pos - 1, true );
        Rules* rule = rules[ pos ];
        rules[ pos ] = rules[ pos - 1 ];
        rules[ pos - 1 ] = rule;
        }
    emit changed( true );
    }

void KCMRulesList::movedownClicked()
    {
    int pos = rules_listbox->currentItem();
    assert( pos != -1 );
    if( pos < int( rules_listbox->count()) - 1 )
        {
        QString txt = rules_listbox->text( pos );
        rules_listbox->removeItem( pos );
        rules_listbox->insertItem( txt, pos + 1 );
        rules_listbox->setSelected( pos + 1, true );
        Rules* rule = rules[ pos ];
        rules[ pos ] = rules[ pos + 1 ];
        rules[ pos + 1 ] = rule;
        }
    emit changed( true );
    }

void KCMRulesList::load()
    {
    rules_listbox->clear();
    for( QVector< Rules* >::Iterator it = rules.begin();
         it != rules.end();
         ++it )
        delete *it;
    rules.clear();
    KConfig cfg( "kwinrulesrc", true );
    cfg.setGroup( "General" );
    int count = cfg.readEntry( "count",0 );
    rules.reserve( count );
    for( int i = 1;
         i <= count;
         ++i )
        {
        cfg.setGroup( QString::number( i ));
        Rules* rule = new Rules( cfg );
        rules.append( rule );
        rules_listbox->insertItem( rule->description );
        }
    if( rules.count() > 0 )
        rules_listbox->setSelected( 0, true );
    else
        activeChanged( NULL );
    }

void KCMRulesList::save()
    {
    KConfig cfg( "kwinrulesrc" );
    QStringList groups = cfg.groupList();
    for( QStringList::ConstIterator it = groups.begin();
         it != groups.end();
         ++it )
        cfg.deleteGroup( *it );
    cfg.setGroup( "General" );
    cfg.writeEntry( "count", rules.count());
    int i = 1;
    for( QVector< Rules* >::ConstIterator it = rules.begin();
         it != rules.end();
         ++it )
        {
        cfg.setGroup( QString::number( i ));
        (*it)->write( cfg );
        ++i;
        }
    }

void KCMRulesList::defaults()
    {
    load();
    }

} // namespace

#include "ruleslist.moc"
