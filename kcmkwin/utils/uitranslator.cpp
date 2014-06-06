/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

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
#include "uitranslator.h"
// frameworks
#include <KLocalizedString>
// Qt
#include <QComboBox>
#include <QMetaObject>
#include <QMetaProperty>

namespace KWin
{

KLocalizedTranslator::KLocalizedTranslator(QObject* parent)
    : QTranslator(parent)
{
}

void KLocalizedTranslator::setTranslationDomain(const QString &translationDomain)
{
    m_translationDomain = translationDomain;
}

void KLocalizedTranslator::addContextToMonitor(const QString &context)
{
    m_monitoredContexts.insert(context);
}

QString KLocalizedTranslator::translate(const char *context, const char *sourceText, const char *disambiguation, int n) const
{
    if (m_translationDomain.isEmpty() || !m_monitoredContexts.contains(QString::fromUtf8(context))) {
        return QTranslator::translate(context, sourceText, disambiguation, n);
    }
    if (qstrlen(disambiguation) == 0) {
        return ki18nd(m_translationDomain.toUtf8().constData(), sourceText).toString();
    } else {
        return ki18ndc(m_translationDomain.toUtf8().constData(), disambiguation, sourceText).toString();
    }
}

} // namespace KWin
