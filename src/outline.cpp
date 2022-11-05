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
#include "scripting/scripting.h"
#include "utils/common.h"
// Frameworks
#include <KConfigGroup>
// Qt
#include <QDebug>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QStandardPaths>

namespace KWin
{

Outline::Outline()
    : m_active(false)
{
    connect(Compositor::self(), &Compositor::compositingToggled, this, &Outline::compositingChanged);
}

Outline::~Outline() = default;

void Outline::show()
{
    if (!m_visual) {
        createHelper();
    }
    if (!m_visual) {
        // something went wrong
        return;
    }
    m_visual->show();
    m_active = true;
    Q_EMIT activeChanged();
}

void Outline::hide()
{
    if (!m_active) {
        return;
    }
    m_active = false;
    Q_EMIT activeChanged();
    if (!m_visual) {
        return;
    }
    m_visual->hide();
}

void Outline::show(const QRect &outlineGeometry)
{
    show(outlineGeometry, QRect());
}

void Outline::show(const QRect &outlineGeometry, const QRect &visualParentGeometry)
{
    setGeometry(outlineGeometry);
    setVisualParentGeometry(visualParentGeometry);
    show();
}

void Outline::setGeometry(const QRect &outlineGeometry)
{
    if (m_outlineGeometry == outlineGeometry) {
        return;
    }
    m_outlineGeometry = outlineGeometry;
    Q_EMIT geometryChanged();
    Q_EMIT unifiedGeometryChanged();
}

void Outline::setVisualParentGeometry(const QRect &visualParentGeometry)
{
    if (m_visualParentGeometry == visualParentGeometry) {
        return;
    }
    m_visualParentGeometry = visualParentGeometry;
    Q_EMIT visualParentGeometryChanged();
    Q_EMIT unifiedGeometryChanged();
}

QRect Outline::unifiedGeometry() const
{
    return m_outlineGeometry | m_visualParentGeometry;
}

void Outline::createHelper()
{
    if (m_visual) {
        return;
    }
    m_visual = kwinApp()->createOutline(this);
}

void Outline::compositingChanged()
{
    m_visual.reset();
    if (m_active) {
        show();
    }
}

const QRect &Outline::geometry() const
{
    return m_outlineGeometry;
}

const QRect &Outline::visualParentGeometry() const
{
    return m_visualParentGeometry;
}

bool Outline::isActive() const
{
    return m_active;
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
    if (QQuickWindow *w = qobject_cast<QQuickWindow *>(m_mainItem.get())) {
        w->hide();
        w->destroy();
    }
}

void CompositedOutlineVisual::show()
{
    if (!m_qmlContext) {
        m_qmlContext = std::make_unique<QQmlContext>(Scripting::self()->qmlEngine());
        m_qmlContext->setContextProperty(QStringLiteral("outline"), m_outline);
    }
    if (!m_qmlComponent) {
        m_qmlComponent = std::make_unique<QQmlComponent>(Scripting::self()->qmlEngine());
        const QString fileName = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                                        kwinApp()->config()->group(QStringLiteral("Outline")).readEntry("QmlPath", QStringLiteral("kwin/outline/plasma/outline.qml")));
        if (fileName.isEmpty()) {
            qCDebug(KWIN_CORE) << "Could not locate outline.qml";
            return;
        }
        m_qmlComponent->loadUrl(QUrl::fromLocalFile(fileName));
        if (m_qmlComponent->isError()) {
            qCDebug(KWIN_CORE) << "Component failed to load: " << m_qmlComponent->errors();
        } else {
            m_mainItem.reset(m_qmlComponent->create(m_qmlContext.get()));
        }
        if (auto w = qobject_cast<QQuickWindow *>(m_mainItem.get())) {
            w->setProperty("__kwin_outline", true);
        }
    }
}

} // namespace
