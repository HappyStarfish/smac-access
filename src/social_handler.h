#pragma once

#include "main.h"

namespace SocialEngHandler {

bool IsActive();
bool Update(UINT msg, WPARAM wParam);
void RunModal();
void RunModalAlloc(); // F3: energy allocation only

}
