/*
  'Web' kwin client

  Copyright (C) 2001 Rik Hemsley (rikkus) <rik@kde.org>

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

#include "WebButtonHelp.h"

using namespace KWinInternal;

static unsigned char help_bits[] =
{
   0x18, 0x18, 0x00, 0x1c, 0x18, 0x18, 0x18, 0x3c
};

WebButtonHelp::WebButtonHelp(QWidget * parent)
  : WebButton(parent, i18n("Help"))
{
  QBitmap b(8, 8, help_bits, true /* isXBitmap */);
  b.setMask(b);
  setBitmap(b);
}

  void
WebButtonHelp::clickEvent(int /* button */)
{
  emit(help());
}

#include "WebButtonHelp.moc"
// vim:ts=2:sw=2:tw=78:set et:
