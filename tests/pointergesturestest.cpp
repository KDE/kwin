/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickView>

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/pointergestures.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/seat.h>

using namespace KWayland::Client;

class PinchGesture : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(qreal scale READ scale NOTIFY scaleChanged)
    Q_PROPERTY(qreal progressScale READ progressScale NOTIFY progressScaleChanged)

public:
    explicit PinchGesture(QQuickItem *parent = nullptr);
    ~PinchGesture() override;

    qreal scale() const
    {
        return m_scale;
    }

    qreal progressScale() const
    {
        return m_progressScale;
    }

protected:
    void componentComplete() override;

Q_SIGNALS:
    void progressScaleChanged();

private:
    void initWayland();
    void setupGesture();

    Pointer *m_pointer = nullptr;
    PointerGestures *m_pointerGestures = nullptr;
    PointerPinchGesture *m_gesture = nullptr;

    qreal m_scale = 1.0;
    qreal m_progressScale = 1.0;
};

PinchGesture::PinchGesture(QQuickItem *parent)
    : QQuickItem(parent)
{
}

PinchGesture::~PinchGesture() = default;

void PinchGesture::componentComplete()
{
    QQuickItem::componentComplete();
    initWayland();
}

void PinchGesture::initWayland()
{
    auto c = ConnectionThread::fromApplication(this);
    Registry *r = new Registry(c);
    r->create(c);

    connect(r, &Registry::interfacesAnnounced, this,
            [this, r] {
                const auto gi = r->interface(Registry::Interface::PointerGesturesUnstableV1);
                if (gi.name == 0) {
                    return;
                }
                m_pointerGestures = r->createPointerGestures(gi.name, gi.version, this);

                // now create seat
                const auto si = r->interface(Registry::Interface::Seat);
                if (si.name == 0) {
                    return;
                }
                auto seat = r->createSeat(si.name, si.version, this);
                connect(seat, &Seat::hasKeyboardChanged, this,
                        [this, seat](bool hasPointer) {
                            if (hasPointer) {
                                m_pointer = seat->createPointer(this);
                                setupGesture();
                            } else {
                                delete m_pointer;
                                delete m_gesture;
                                m_pointer = nullptr;
                                m_gesture = nullptr;
                            }
                        });
            });

    r->setup();
    c->roundtrip();
}

void PinchGesture::setupGesture()
{
    if (m_gesture || !m_pointerGestures || !m_pointer) {
        return;
    }
    m_gesture = m_pointerGestures->createPinchGesture(m_pointer, this);
    connect(m_gesture, &PointerPinchGesture::updated, this,
            [this](const QSizeF &delta, qreal scale) {
                m_progressScale = scale;
                Q_EMIT progressScaleChanged();
            });
    connect(m_gesture, &PointerPinchGesture::ended, this,
            [this] {
                m_scale = m_scale * m_progressScale;
                m_progressScale = 1.0;
                Q_EMIT scaleChanged();
                Q_EMIT progressScaleChanged();
            });
    connect(m_gesture, &PointerPinchGesture::cancelled, this,
            [this] {
                m_progressScale = 1.0;
                Q_EMIT progressScaleChanged();
            });
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("wayland"));
    QGuiApplication app(argc, argv);

    qmlRegisterType<PinchGesture>("org.kde.kwin.tests", 1, 0, "PinchGesture");

    QQuickView view;
    view.setSource(QUrl::fromLocalFile(QStringLiteral(DIR) + QStringLiteral("/pointergesturestest.qml")));

    view.show();

    return app.exec();
}

#include "pointergesturestest.moc"
