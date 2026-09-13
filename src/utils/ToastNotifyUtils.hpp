#pragma once
#include <windows.h>
#include <string>

namespace ToastNotifyUtils {

void ShowNotification(const std::string& title, const std::string& message);

}
