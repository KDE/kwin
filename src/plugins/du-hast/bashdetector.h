#pragma once

#include "input_event.h"

#include <deque>

namespace KWin
{

class BashDetector
{
public:
    BashDetector();
    ~BashDetector();

    bool update(KeyEvent *event);

private:
    struct HistoryItem
    {
        quint32 keyCode;
        std::chrono::microseconds timestamp;
    };

    std::deque<HistoryItem> m_history;
};

} // namespace KWin
