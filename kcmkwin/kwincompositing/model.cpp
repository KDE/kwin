/**************************************************************************
* KWin - the KDE window manager                                          *
* This file is part of the KDE project.                                  *
*                                                                        *
* Copyright (C) 2013 Antonis Tsiapaliokas <kok3rs@gmail.com>             *
*                                                                        *
* This program is free software; you can redistribute it and/or modify   *
* it under the terms of the GNU General Public License as published by   *
* the Free Software Foundation; either version 2 of the License, or      *
* (at your option) any later version.                                    *
*                                                                        *
* This program is distributed in the hope that it will be useful,        *
* but WITHOUT ANY WARRANTY; without even the implied warranty of         *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
* GNU General Public License for more details.                           *
*                                                                        *
* You should have received a copy of the GNU General Public License      *
* along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
**************************************************************************/

#include "model.h"
#include "effectconfig.h"
#include "effectmodel.h"
#include "compositing.h"
#include <config-kwin.h>
#include <kwin_effects_interface.h>
#include <effect_builtins.h>

#include <KSharedConfig>
#include <KCModuleProxy>
#include <kdeclarative/kdeclarative.h>

#include <QVariant>
#include <QString>
#include <QQmlEngine>
#include <QtQml>
#include <QQuickItem>
#include <QDebug>

namespace KWin {
namespace Compositing {

EffectFilterModel::EffectFilterModel(QObject *parent)
    : QSortFilterProxyModel(parent)
    , m_effectModel(new EffectModel(this))
    , m_filterOutUnsupported(true)
    , m_filterOutInternal(true)
{
    setSourceModel(m_effectModel);
    connect(this, &EffectFilterModel::filterOutUnsupportedChanged, this, &EffectFilterModel::invalidateFilter);
    connect(this, &EffectFilterModel::filterOutInternalChanged, this, &EffectFilterModel::invalidateFilter);
}

const QString &EffectFilterModel::filter() const
{
    return m_filter;
}

void EffectFilterModel::setFilter(const QString &filter)
{
    if (filter == m_filter) {
        return;
    }

    m_filter = filter;
    emit filterChanged();
    invalidateFilter();
}

bool EffectFilterModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    if (!m_effectModel) {
        return false;
    }

    QModelIndex index = m_effectModel->index(source_row, 0, source_parent);
    if (!index.isValid()) {
        return false;
    }

    if (m_filterOutUnsupported) {
        if (!index.data(EffectModel::SupportedRole).toBool()) {
            return false;
        }
    }

    if (m_filterOutInternal) {
        if (index.data(EffectModel::InternalRole).toBool()) {
            return false;
        }
    }

    if (m_filter.isEmpty()) {
        return true;
    }

    QVariant data = index.data();
    if (!data.isValid()) {
        //An invalid QVariant is valid data
        return true;
    }

    if (m_effectModel->data(index, EffectModel::NameRole).toString().contains(m_filter, Qt::CaseInsensitive)) {
        return true;
    } else if (m_effectModel->data(index, EffectModel::DescriptionRole).toString().contains(m_filter, Qt::CaseInsensitive)) {
        return true;
    }
    if (index.data(EffectModel::CategoryRole).toString().contains(m_filter, Qt::CaseInsensitive)) {
        return true;
    }

    return false;
}

void EffectFilterModel::updateEffectStatus(int rowIndex, int effectState)
{
    const QModelIndex sourceIndex = mapToSource(index(rowIndex, 0));

    m_effectModel->updateEffectStatus(sourceIndex, EffectModel::Status(effectState));
}

void EffectFilterModel::syncConfig()
{
    m_effectModel->save();
}

void EffectFilterModel::load()
{
    m_effectModel->load();
}

void EffectFilterModel::defaults()
{
    m_effectModel->defaults();
}

EffectView::EffectView(ViewType type, QWidget *parent)
    : QQuickWidget(parent)
{
    qRegisterMetaType<OpenGLPlatformInterfaceModel*>();
    qmlRegisterType<EffectConfig>("org.kde.kwin.kwincompositing", 1, 0, "EffectConfig");
    qmlRegisterType<EffectFilterModel>("org.kde.kwin.kwincompositing", 1, 0, "EffectFilterModel");
    qmlRegisterType<Compositing>("org.kde.kwin.kwincompositing", 1, 0, "Compositing");
    qmlRegisterType<CompositingType>("org.kde.kwin.kwincompositing", 1, 0, "CompositingType");
    init(type);
}

void EffectView::init(ViewType type)
{
    KDeclarative::KDeclarative kdeclarative;
    kdeclarative.setDeclarativeEngine(engine());
    kdeclarative.setTranslationDomain(QStringLiteral(TRANSLATION_DOMAIN));
    kdeclarative.setupContext();
    kdeclarative.setupEngine(engine());
    QString path;
    switch (type) {
    case CompositingSettingsView:
        path = QStringLiteral("kwincompositing/qml/main-compositing.qml");
        break;
    case DesktopEffectsView:
        path = QStringLiteral("kwincompositing/qml/main.qml");
        break;
    }
    QString mainFile = QStandardPaths::locate(QStandardPaths::GenericDataLocation, path, QStandardPaths::LocateFile);
    setResizeMode(QQuickWidget::SizeRootObjectToView);
    setSource(QUrl(mainFile));
    rootObject()->setProperty("color",
                              KColorScheme(QPalette::Active, KColorScheme::Window, KSharedConfigPtr(0)).background(KColorScheme::NormalBackground).color());
    connect(rootObject(), SIGNAL(changed()), this, SIGNAL(changed()));
    setMinimumSize(initialSize());
    connect(rootObject(), SIGNAL(implicitWidthChanged()), this, SLOT(slotImplicitSizeChanged()));
    connect(rootObject(), SIGNAL(implicitHeightChanged()), this, SLOT(slotImplicitSizeChanged()));
}

void EffectView::save()
{
    if (auto *model = rootObject()->findChild<EffectFilterModel*>(QStringLiteral("filterModel"))) {
        model->syncConfig();
    }
    if (auto *compositing = rootObject()->findChild<Compositing*>(QStringLiteral("compositing"))) {
        compositing->save();
    }
}

void EffectView::load()
{
    if (auto *model = rootObject()->findChild<EffectFilterModel*>(QStringLiteral("filterModel"))) {
        model->load();
    }
    if (auto *compositing = rootObject()->findChild<Compositing*>(QStringLiteral("compositing"))) {
        compositing->reset();
    }
}

void EffectView::defaults()
{
    if (auto *model = rootObject()->findChild<EffectFilterModel*>(QStringLiteral("filterModel"))) {
        model->defaults();
    }
    if (auto *compositing = rootObject()->findChild<Compositing*>(QStringLiteral("compositing"))) {
        compositing->defaults();
    }
}

void EffectView::slotImplicitSizeChanged()
{
    setMinimumSize(QSize(rootObject()->property("implicitWidth").toInt(),
                         rootObject()->property("implicitHeight").toInt()));
}

}//end namespace Compositing
}//end namespace KWin
