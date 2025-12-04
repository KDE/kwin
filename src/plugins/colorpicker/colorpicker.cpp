/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "colorpicker.h"
#include "colorpickerlayer.h"

#include "compositor.h"
#include "core/output.h"
#include "core/pixelgrid.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "effect/effect.h"
#include "effect/effecthandler.h"
#include "opengl/eglbackend.h"
#include "opengl/glplatform.h"
#include "opengl/glutils.h"
#include "scene/item.h"
#include "scene/itemrenderer.h"
#include "scene/windowitem.h"
#include "scene/workspacescene.h"
#include "window.h"
#include "workspace.h"

#include <KLocalizedString>
#include <QDBusConnection>
#include <QDBusMetaType>

Q_DECLARE_METATYPE(QColor)

QDBusArgument &operator<<(QDBusArgument &argument, const QColor &color)
{
    argument.beginStructure();
    argument << color.rgba();
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, QColor &color)
{
    argument.beginStructure();
    QRgb rgba;
    argument >> rgba;
    argument.endStructure();
    color = QColor::fromRgba(rgba);
    return argument;
}

namespace KWin
{

bool ColorPickerEffect::supported()
{
    return effects->isOpenGLCompositing();
}

ColorPickerEffect::ColorPickerEffect()
{
    qDBusRegisterMetaType<QColor>();
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/ColorPicker"), this, QDBusConnection::ExportScriptableContents);
}

ColorPickerEffect::~ColorPickerEffect()
{
    setPicking(false);
}

QColor ColorPickerEffect::pick()
{
    if (!calledFromDBus()) {
        return QColor();
    }
    if (m_picking) {
        sendErrorReply(QDBusError::Failed, "Color picking is already in progress");
        return QColor();
    }
    setPicking(true);
    m_replyMessage = message();
    setDelayedReply(true);
    showInfoMessage();
    effects->startInteractivePositionSelection(
        [this](const QPointF &p) {
            hideInfoMessage();
            if (p == QPointF(-1, -1)) {
                // error condition
                QDBusConnection::sessionBus().send(m_replyMessage.createErrorReply(QStringLiteral("org.kde.kwin.ColorPicker.Error.Cancelled"), "Color picking got cancelled"));
                setPicking(false);
            } else {
                const auto turnOffPicking = qScopeGuard([this] {
                    setPicking(false);
                });

                const auto eglBackend = dynamic_cast<EglBackend *>(Compositor::self()->backend());
                if (!eglBackend) {
                    return;
                }
                const auto context = eglBackend->openglContext();
                if (!context || !context->makeCurrent()) {
                    return;
                }

                const auto offscreenTexture = GLTexture::allocate(GL_RGB8, QSize(1, 1));
                if (!offscreenTexture) {
                    return;
                }
                const auto target = std::make_unique<GLFramebuffer>(offscreenTexture.get());
                if (!target->valid()) {
                    return;
                }

                auto screen = effects->screenAt(p.toPoint());
                if (!screen) {
                    return;
                }

                ColorPickerLayer layer(screen->backendOutput(), target.get());
                if (!layer.preparePresentationTest()) {
                    return;
                }
                const auto beginInfo = layer.beginFrame();
                if (!beginInfo) {
                    return;
                }
                SceneView sceneView(Compositor::self()->scene(), screen, nullptr, &layer);
                auto cursorView = std::make_unique<ItemTreeView>(&sceneView, Compositor::self()->scene()->cursorItem(), workspace()->outputs().front(), nullptr, nullptr);
                cursorView->setExclusive(true);
                const QRect pixelDamage = QRect(QPoint(), QSize(1, 1));
                sceneView.setViewport(QRectF(p, QSizeF(1, 1)));
                sceneView.prePaint();
                sceneView.paint(beginInfo->renderTarget, QPoint(), pixelDamage);
                sceneView.postPaint();
                if (!layer.endFrame(pixelDamage, pixelDamage, nullptr)) {
                    return;
                }

                GLFramebuffer::pushFramebuffer(target.get());
                QImage snapshot = QImage(offscreenTexture->size(), QImage::Format_RGB888);
                context->glReadnPixels(0, 0, snapshot.width(), snapshot.height(), GL_RGB, GL_UNSIGNED_BYTE, snapshot.sizeInBytes(), static_cast<GLvoid *>(snapshot.bits()));
                GLFramebuffer::popFramebuffer();

                QDBusConnection::sessionBus().send(m_replyMessage.createReply(snapshot.pixelColor(0, 0)));
            }
        });
    return QColor();
}

void ColorPickerEffect::showInfoMessage()
{
    effects->showOnScreenMessage(i18n("Select a position for color picking with left click or enter.\nEscape or right click to cancel."), QStringLiteral("color-picker"));
}

void ColorPickerEffect::hideInfoMessage()
{
    effects->hideOnScreenMessage();
}

void ColorPickerEffect::setPicking(bool picking)
{
    if (m_picking != picking) {
        m_picking = picking;
        Q_EMIT effects->colorPickerActiveChanged();
    }
}

bool ColorPickerEffect::isActive() const
{
    return m_picking && !effects->isScreenLocked();
}

} // namespace

#include "moc_colorpicker.cpp"
