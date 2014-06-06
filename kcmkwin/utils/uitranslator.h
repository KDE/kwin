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
#ifndef KWIN_KCMUTILS_UITRANSLATOR_H
#define KWIN_KCMUTILS_UITRANSLATOR_H

#include <QSet>
#include <QTranslator>

class QWidget;

namespace KWin
{

class KLocalizedTranslator : public QTranslator
{
    Q_OBJECT
public:
    explicit KLocalizedTranslator(QObject *parent = 0);
    QString translate(const char *context, const char *sourceText, const char *disambiguation = 0, int n = -1) const override;

    void setTranslationDomain(const QString &translationDomain);
    void addContextToMonitor(const QString &context);

private:
    QString m_translationDomain;
    QSet<QString> m_monitoredContexts;
};

}

#endif
