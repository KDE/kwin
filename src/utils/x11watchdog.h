#pragma once
#include "kwin_export.h"

#include <condition_variable>
#include <thread>

namespace KWin
{

class KWIN_EXPORT X11Watchdog
{
public:
    explicit X11Watchdog();
    ~X11Watchdog();
};

class KWIN_EXPORT X11WatchdogThread
{
public:
    explicit X11WatchdogThread();
    ~X11WatchdogThread();

    void start();
    void stop();

private:
    std::jthread m_thread;
    int m_safeguardCounter = 0;
    std::condition_variable m_interrupt;
    std::mutex m_mutex;
};

}
