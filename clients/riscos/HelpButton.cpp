/*
  RISC OS KWin client
  
  Copyright 2000
    Rik Hemsley <rik@kde.org>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING.  If not, write to
  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/

#include "HelpButton.h"

namespace RiscOS
{

/* XPM */
static const char * const help_xpm[] = {
"12 12 3 1",
" 	c None",
".	c #FFFFFF",
"+	c #000000",
" .  ++++  . ",
"  .+....+.  ",
"  +......+  ",
"..+......+..",
"  +......+  ",
"  +......+  ",
"  .+....+.  ",
" .  ++ +  . ",
"    + ++    ",
"    ++ +    ",
"    + ++    ",
"     ++     "};

HelpButton::HelpButton(QWidget * parent)
  : Button(parent, i18n("Help"))
{
  setPixmap(QPixmap((const char **)help_xpm));
  connect(this, SIGNAL(help()), parent, SLOT(slotHelp()));
}

  void
HelpButton::mouseReleaseEvent(QMouseEvent * e)
{
  Button::mouseReleaseEvent(e);

  if (!rect().contains(e->pos()))
    return;

  switch (e->button())
  {
    default:
      emit(help());
      break;
  }

}
} // End namespace;

// vim:ts=2:sw=2:tw=78
#include "HelpButton.moc"
