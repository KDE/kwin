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

#ifndef RISC_OS_PALETTE_H
#define RISC_OS_PALETTE_H

#include <qarray.h>
#include <qglobal.h>

namespace RiscOS
{

class Palette
{
  public:

    Palette()
    {
      data_.resize(8);

      data_[0] = 0xFFFFFFFF;
      data_[1] = 0xFFDCDCDC;
      data_[2] = 0xFFC3C3C3;
      data_[3] = 0xFFA0A0A0;
      data_[4] = 0xFF808080;
      data_[5] = 0xFF585858;
      data_[6] = 0xFF303030;
      data_[7] = 0xFF000000;
    }

    QRgb & operator [] (int idx)
    {
      return data_[idx];
    }

    QRgb operator [] (int idx) const
    {
      return data_[idx];
    }

  private:

    QArray<QRgb> data_;
};

} // End namespace

#endif

// vim:ts=2:sw=2:tw=78
