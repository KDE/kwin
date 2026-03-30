#pragma once

#include "kwin_export.h"

#include "clientconnection.h"

#include <QObject>

#include <memory>

namespace KWin
{

class ScreencastStreamV2InterfacePrivate;
class ScreencastManagerV2InterfacePrivate;
class ScreencastOutputParamsV2Private;
class ScreencastWindowParamsV2Private;
class ScreencastRegionParamsV2Private;
class ScreencastVirtualOutputParamsV2Private;

class KWIN_EXPORT ScreencastStreamV2Interface : public QObject
{
    Q_OBJECT

public:
    ~ScreencastStreamV2Interface() override;

    enum class CursorMode {
        Hidden = 1,
        Embedded = 2,
        Metadata = 4,
    };

    void sendCreated(quint32 nodeid, quint64 objectSerial);
    void sendFailed(const QString &error);
    void sendClosed();

    ClientConnection *connection() const;

Q_SIGNALS:
    void finished();

private:
    explicit ScreencastStreamV2Interface();
    std::unique_ptr<ScreencastStreamV2InterfacePrivate> d;
    friend class ScreencastOutputParamsV2Private;
    friend class ScreencastWindowParamsV2Private;
    friend class ScreencastRegionParamsV2Private;
    friend class ScreencastVirtualOutputParamsV2Private;
};

class KWIN_EXPORT ScreencastOutputParamsV2
{
public:
    ~ScreencastOutputParamsV2();
    QString output() const;
    ScreencastStreamV2Interface::CursorMode cursorMode() const;

private:
    explicit ScreencastOutputParamsV2(ScreencastOutputParamsV2Private *d);
    std::unique_ptr<ScreencastOutputParamsV2Private> d;
    friend class ScreencastOutputParamsV2Private;
};

class KWIN_EXPORT ScreencastWindowParamsV2
{
public:
    ~ScreencastWindowParamsV2();
    QString window() const;
    ScreencastStreamV2Interface::CursorMode cursorMode() const;

private:
    explicit ScreencastWindowParamsV2(ScreencastWindowParamsV2Private *d);
    std::unique_ptr<ScreencastWindowParamsV2Private> d;
    friend class ScreencastWindowParamsV2Private;
};

class KWIN_EXPORT ScreencastRegionParamsV2
{
public:
    ~ScreencastRegionParamsV2();
    QRect region() const;
    ScreencastStreamV2Interface::CursorMode cursorMode() const;
    double scale() const;

private:
    explicit ScreencastRegionParamsV2(ScreencastRegionParamsV2Private *d);
    std::unique_ptr<ScreencastRegionParamsV2Private> d;
    friend class ScreencastRegionParamsV2Private;
};

class KWIN_EXPORT ScreencastVirtualOutputParamsV2
{
public:
    ~ScreencastVirtualOutputParamsV2();
    QSize size() const;
    double scale() const;
    ScreencastStreamV2Interface::CursorMode cursorMode() const;
    QString name() const;
    QString description() const;

private:
    explicit ScreencastVirtualOutputParamsV2(ScreencastVirtualOutputParamsV2Private *d);
    std::unique_ptr<ScreencastVirtualOutputParamsV2Private> d;
    friend class ScreencastVirtualOutputParamsV2Private;
};

class KWIN_EXPORT ScreencastManagerV2Interface : public QObject
{
    Q_OBJECT

public:
    explicit ScreencastManagerV2Interface(Display *display, QObject *parent = nullptr);
    ~ScreencastManagerV2Interface() override;
Q_SIGNALS:
    void outputScreencastRequested(ScreencastStreamV2Interface *stream, const ScreencastOutputParamsV2 &params);
    void windowScreencastRequested(ScreencastStreamV2Interface *stream, const ScreencastWindowParamsV2 &params);
    void regionScreencastRequested(ScreencastStreamV2Interface *stream, const ScreencastRegionParamsV2 &params);
    void virtualOutputScreencastRequested(ScreencastStreamV2Interface *stream, const ScreencastVirtualOutputParamsV2 &params);

private:
    std::unique_ptr<ScreencastManagerV2InterfacePrivate> d;
};

}
