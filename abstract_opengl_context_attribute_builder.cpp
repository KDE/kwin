/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "abstract_opengl_context_attribute_builder.h"

namespace KWin
{

QDebug AbstractOpenGLContextAttributeBuilder::operator<<(QDebug dbg) const
{
    QDebugStateSaver saver(dbg);
    dbg.nospace() << "\nVersion requested:\t" << isVersionRequested() << "\n";
    if (isVersionRequested()) {
        dbg.nospace() << "Version:\t" << majorVersion() << "." << minorVersion() << "\n";
    }
    dbg.nospace() << "Robust:\t" << isRobust() << "\n";
    dbg.nospace() << "Forward compatible:\t" << isForwardCompatible() << "\n";
    dbg.nospace() << "Core profile:\t" << isCoreProfile() << "\n";
    dbg.nospace() << "Compatibility profile:\t" << isCompatibilityProfile();
    return dbg;
}

}
