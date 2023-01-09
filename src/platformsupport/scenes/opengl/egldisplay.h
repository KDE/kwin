#pragma once

#include "kwin_export.h"

#include <QByteArray>
#include <QList>
#include <epoxy/egl.h>

namespace KWin
{

class KWIN_EXPORT KWinEglDisplay
{
public:
    bool init(EGLDisplay display);

    QList<QByteArray> extensions() const;
    EGLDisplay display() const;
    bool isValid() const;
    bool hasExtension(const QByteArray &name) const;

    bool supportsBufferAge() const;
    bool supportsPartialUpdate() const;
    bool supportsSwapBuffersWithDamage() const;

private:
    EGLDisplay m_display = EGL_NO_DISPLAY;
    QList<QByteArray> m_extensions;

    bool m_supportsBufferAge = false;
    bool m_supportsPartialUpdate = false;
    bool m_supportsSwapBuffersWithDamage = false;
};

}
