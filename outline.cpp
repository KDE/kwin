/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2011 Arthur Arlt <a.arlt@stud.uni-heidelberg.de>
    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
// own
#include "outline.h"
// KWin
#include "composite.h"
#include "main.h"
#include "platform.h"
#include "scripting/scripting.h"
#include "utils.h"
// Frameworks
#include <KConfigGroup>
// Qt
#include <QDebug>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QStandardPaths>

namespace KWin {

KWIN_SINGLETON_FACTORY(Outline)

Outline::Outline(QObject *parent)
    : QObject(parent)
    , m_active(false)
{
    connect(Compositor::self(), SIGNAL(compositingToggled(bool)), SLOT(compositingChanged()));
}

Outline::~Outline()
{
}

void Outline::show()
{
    if (m_visual.isNull()) {
        createHelper();
    }
    if (m_visual.isNull()) {
        // something went wrong
        return;
    }
    m_visual->show();
    m_active = true;
    emit activeChanged();
}

void Outline::hide()
{
    if (!m_active) {
        return;
    }
    m_active = false;
    emit activeChanged();
    if (m_visual.isNull()) {
        return;
    }
    m_visual->hide();
}

void Outline::show(const QRect& outlineGeometry)
{
    show(outlineGeometry, QRect());
}

void Outline::show(const QRect &outlineGeometry, const QRect &visualParentGeometry)
{
    setGeometry(outlineGeometry);
    setVisualParentGeometry(visualParentGeometry);
    show();
}

void Outline::setGeometry(const QRect& outlineGeometry)
{
    if (m_outlineGeometry == outlineGeometry) {
        return;
    }
    m_outlineGeometry = outlineGeometry;
    emit geometryChanged();
    emit unifiedGeometryChanged();
}

void Outline::setVisualParentGeometry(const QRect &visualParentGeometry)
{
    if (m_visualParentGeometry == visualParentGeometry) {
        return;
    }
    m_visualParentGeometry = visualParentGeometry;
    emit visualParentGeometryChanged();
    emit unifiedGeometryChanged();
}

QRect Outline::unifiedGeometry() const
{
    return m_outlineGeometry | m_visualParentGeometry;
}

void Outline::createHelper()
{
    if (!m_visual.isNull()) {
        return;
    }
    m_visual.reset(kwinApp()->platform()->createOutline(this));
}

void Outline::compositingChanged()
{
    m_visual.reset();
    if (m_active) {
        show();
    }
}

OutlineVisual::OutlineVisual(Outline *outline)
    : m_outline(outline)
{
}

OutlineVisual::~OutlineVisual()
{
}

CompositedOutlineVisual::CompositedOutlineVisual(Outline *outline)
    : OutlineVisual(outline)
    , m_qmlContext()
    , m_qmlComponent()
    , m_mainItem()
{
}

CompositedOutlineVisual::~CompositedOutlineVisual()
{
}

void CompositedOutlineVisual::hide()
{
    if (QQuickWindow *w = qobject_cast<QQuickWindow*>(m_mainItem.data())) {
        w->hide();
        w->destroy();
    }
}

void CompositedOutlineVisual::show()
{
    if (m_qmlContext.isNull()) {
        m_qmlContext.reset(new QQmlContext(Scripting::self()->qmlEngine()));
        m_qmlContext->setContextProperty(QStringLiteral("outline"), outline());
    }
    if (m_qmlComponent.isNull()) {
        m_qmlComponent.reset(new QQmlComponent(Scripting::self()->qmlEngine()));
        const QString fileName = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                                 kwinApp()->config()->group(QStringLiteral("Outline")).readEntry("QmlPath", QStringLiteral(KWIN_NAME "/outline/plasma/outline.qml")));
        if (fileName.isEmpty()) {
            qCDebug(KWIN_CORE) << "Could not locate outline.qml";
            return;
        }
        m_qmlComponent->loadUrl(QUrl::fromLocalFile(fileName));
        if (m_qmlComponent->isError()) {
            qCDebug(KWIN_CORE) << "Component failed to load: " << m_qmlComponent->errors();
        } else {
            m_mainItem.reset(m_qmlComponent->create(m_qmlContext.data()));
        }
        if (auto w = qobject_cast<QQuickWindow *>(m_mainItem.data())) {
            w->setProperty("__kwin_outline", true);
        }
    }
}

} // namespace
