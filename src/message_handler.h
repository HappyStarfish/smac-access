#pragma once

#include "main.h"

namespace MessageHandler {
    /// Capture a message and auto-announce it via screen reader.
    /// Called before NetMsg_pop in mod source files.
    bool OnMessage(const char* label, const char* filename);
    /// Open modal message log browser (Ctrl+M).
    void OpenBrowser();
    /// Number of messages in the buffer.
    int GetCount();
    /// Clear all messages.
    void Clear();
    /// True while the modal browser is open.
    bool IsActive();
}
