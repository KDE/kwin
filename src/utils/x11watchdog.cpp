#include "x11watchdog.h"
#include "main.h"

#include <QtAssert>
#include <signal.h>
#include <stop_token>

using namespace std::chrono_literals;

namespace KWin
{

X11Watchdog::X11Watchdog()
{
    if (kwinApp()->operationMode() != Application::OperationModeX11) {
        kwinApp()->x11Watchdog()->start();
    }
}

X11Watchdog::~X11Watchdog()
{
    if (kwinApp()->operationMode() != Application::OperationModeX11) {
        kwinApp()->x11Watchdog()->stop();
    }
}

X11WatchdogThread::X11WatchdogThread()
{
    m_thread = std::jthread([this](std::stop_token token) {
        std::unique_lock lock(m_mutex);
        while (!token.stop_requested()) {
            if (m_safeguardCounter > 0 && kwinApp()->x11Connection()) {
                auto status = m_interrupt.wait_for(lock, 3s);
                if (status == std::cv_status::timeout) {
                    qCritical() << "X11 watchdog timed out, severing the X11 connection!";
                    close(xcb_get_file_descriptor(kwinApp()->x11Connection()));
                    pthread_kill(m_thread.native_handle(), SIGINT);
                }
            } else {
                m_interrupt.wait(lock);
            }
        }
    });
}

X11WatchdogThread::~X11WatchdogThread()
{
    std::unique_lock lock(m_mutex);
    Q_ASSERT(m_safeguardCounter == 0);
    m_thread.request_stop();
    m_interrupt.notify_one();
}

void X11WatchdogThread::start()
{
    std::unique_lock lock(m_mutex);
    if (m_safeguardCounter == 0) {
        m_interrupt.notify_one();
    }
    m_safeguardCounter++;
}

void X11WatchdogThread::stop()
{
    std::unique_lock lock(m_mutex);
    Q_ASSERT(m_safeguardCounter > 0);
    m_safeguardCounter--;
    if (m_safeguardCounter == 0) {
        m_interrupt.notify_one();
    }
}

}
