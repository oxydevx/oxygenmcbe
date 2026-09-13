#include "Sprint.hpp"
#include <fstream>
#include <sstream>
#include <shlobj.h>
#include <iomanip>

Sprint::Sprint() : Feature("Sprint", Category::Movement) {
    this->Description = "Auto sprint";
    this->Enabled = false;

    LoadSprintKeyFromOptions();

    this->isRunning = true;
    this->workerThread = std::thread(&Sprint::KeySpammerLoop, this);
}

Sprint::~Sprint() {
    StopThread();
}

void Sprint::StopThread() {
    isRunning = false;
    if (workerThread.joinable()) {
        workerThread.join();
    }
}

void Sprint::LoadSprintKeyFromOptions() {
    char appDataPath[MAX_PATH];

    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appDataPath))) {
        std::string fullPath = std::string(appDataPath) + "\\Minecraft Bedrock\\Users\\Shared\\games\\com.mojang\\minecraftpe\\options.txt";

        std::ifstream ifs(fullPath);
        if (ifs.is_open()) {
            std::string line;
            while (std::getline(ifs, line)) {
                if (line.find("keyboard_type_0_key.sprint:") == 0) {
                        size_t colonPos = line.find(':');
                        if (colonPos != std::string::npos) {
                            std::string codeStr = line.substr(colonPos + 1);

                            int mcKeyCode = 0;
                            try {
                                mcKeyCode = std::stoi(codeStr);
                            } catch (...) {
                                mcKeyCode = 0; 
                            }
                            this->sprintKeyVirtualCode = MapMinecraftKeyToVK(mcKeyCode);

                        std::stringstream ss;
                        ss << "Raw:" << codeStr << " -> VK:0x" << std::uppercase << std::hex << this->sprintKeyVirtualCode;
                    }
                    break;
                }
            }
            ifs.close();
        }
    }
}

int Sprint::MapMinecraftKeyToVK(int mcKeyCode) {
    return mcKeyCode;
}

void Sprint::KeySpammerLoop() {
    bool pressing = false;

    while (this->isRunning) {

        CURSORINFO ci = { 0 };
        ci.cbSize = sizeof(CURSORINFO);
        bool isCursorHidden = false;

        if (GetCursorInfo(&ci)) {
            if (ci.flags == 0) {
                isCursorHidden = true;
            }
        }

        bool wantSprint = this->Enabled &&
                          this->sprintKeyVirtualCode != 0 &&
                          (GetAsyncKeyState('W') & 0x8000) &&
                          isCursorHidden;

        if (wantSprint && !pressing) {
            UINT scanCode = MapVirtualKeyA(this->sprintKeyVirtualCode, MAPVK_VK_TO_VSC);
            keybd_event(static_cast<BYTE>(this->sprintKeyVirtualCode), static_cast<BYTE>(scanCode), 0, 0);
            pressing = true;
        }
        else if (!wantSprint && pressing) {
            UINT scanCode = MapVirtualKeyA(this->sprintKeyVirtualCode, MAPVK_VK_TO_VSC);
            keybd_event(static_cast<BYTE>(this->sprintKeyVirtualCode), static_cast<BYTE>(scanCode), KEYEVENTF_KEYUP, 0);
            pressing = false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    if (pressing) {
        UINT scanCode = MapVirtualKeyA(this->sprintKeyVirtualCode, MAPVK_VK_TO_VSC);
        keybd_event(static_cast<BYTE>(this->sprintKeyVirtualCode), static_cast<BYTE>(scanCode), KEYEVENTF_KEYUP, 0);
    }
}

void Sprint::OnEvent() {
}
