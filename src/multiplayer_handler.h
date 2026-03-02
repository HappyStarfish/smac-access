#pragma once

#include "main.h"

namespace MultiplayerHandler {

bool IsActive();
bool Update(UINT msg, WPARAM wParam);
void OnOpen();
void OnClose();
void OnTimer();

}
