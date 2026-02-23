#pragma once

#include "main.h"

enum BaseSection {
    BS_Overview = 0,
    BS_Resources,
    BS_Production,
    BS_Economy,
    BS_Facilities,
    BS_Status,
    BS_Units,
    BS_Count
};

namespace BaseScreenHandler {

bool IsActive();
bool IsPickerActive();
bool IsQueueActive();
bool IsDemolitionActive();
bool Update(UINT msg, WPARAM wParam);
void OnOpen();
void OnClose();
void SetSectionFromTab(int tab_index);
void AnnounceCurrentSection(bool interrupt = true);
const char* GetHelpText();

}
