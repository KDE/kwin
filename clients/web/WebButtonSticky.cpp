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

#include "WebButtonSticky.h"

using namespace KWinInternal;

static unsigned char sticky_bits[] =
{
   0x20, 0x70, 0xfa, 0x7e, 0x3c, 0x1c, 0x32, 0x01
};

static unsigned char unsticky_bits[] =
{
   0x1c, 0x1c, 0x1c, 0x3e, 0x7f, 0x08, 0x08, 0x08
};

WebButtonSticky::WebButtonSticky(bool sticky, QWidget * parent)
  : WebButton(parent, i18n("Sticky"))
{
  QBitmap b(8, 8, sticky ? unsticky_bits : sticky_bits, true /* isXBitmap */);
  b.setMask(b);
  setBitmap(b);
}

  void
WebButtonSticky::slotStickyChange(bool sticky)
{
  QBitmap b(8, 8, sticky ? unsticky_bits : sticky_bits, true /* isXBitmap */);
  b.setMask(b);
  setBitmap(b);
  setTipText(sticky ? i18n("Un-Sticky") : i18n("Sticky"));
}

  void
WebButtonSticky::clickEvent(int /* button */)
{
  emit(toggleSticky());
}

#include "WebButtonSticky.moc"
// vim:ts=2:sw=2:tw=78:set et:
