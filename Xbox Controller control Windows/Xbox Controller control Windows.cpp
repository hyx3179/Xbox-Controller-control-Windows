#include <iostream>
#include <curl/curl.h>
#include <xinput.h>
#include <winuser.h>

#pragma comment( linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"" )

#define MOUSE 0
#define MPC_BE 1

typedef struct KEY_MAP {
    int type;
    WORD gamepad_button;
    DWORD mouse_button_down;
    DWORD mouse_button_up;
    const char *mpc_be_control;
} key_map;

key_map key_map_list[] = {
    {MOUSE,XINPUT_GAMEPAD_A,MOUSEEVENTF_LEFTDOWN,MOUSEEVENTF_LEFTUP,""},
    {MOUSE,XINPUT_GAMEPAD_B,MOUSEEVENTF_RIGHTDOWN,MOUSEEVENTF_RIGHTUP,""},
    {MPC_BE,XINPUT_GAMEPAD_DPAD_UP,0,0,"wm_command=889"},
    {MPC_BE,XINPUT_GAMEPAD_DPAD_LEFT,0,0,"wm_command=901"},
    {MPC_BE,XINPUT_GAMEPAD_DPAD_RIGHT,0,0,"wm_command=902"}
};

DWORD WINAPI mpc_be_control_send(LPVOID lpParam) {

    const char *fromdata = (const char*)lpParam;
    CURL *curl = curl_easy_init();
    struct curl_slist *headerlist = NULL;

    headerlist = curl_slist_append(headerlist,
        "Content-Type:application/x-www-form-urlencoded");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);  // 设置表头
    curl_easy_setopt(curl, CURLOPT_URL,
        "http://localhost:13579/command.html"); // 设置URL
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, fromdata); // 设置参数
    curl_easy_setopt(curl, CURLOPT_POST, 1); // 设置为Post

    curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    curl_slist_free_all(headerlist);

    return 0;
}

void mouse(DWORD UserIndex) {

    XINPUT_STATE state;
    ZeroMemory(&state, sizeof(XINPUT_STATE));
    XINPUT_STATE previous_state = state;
    WORD change = 0;
    INPUT input = { 0 };

    while (true)
    {
        XInputGetState(UserIndex, &state);
        input.type = INPUT_MOUSE;
        input.mi.dx = state.Gamepad.sThumbLX / 4096;
        input.mi.dy = -state.Gamepad.sThumbLY / 4096;
        input.mi.dwFlags = MOUSEEVENTF_MOVE;

        for (int i = 0; i < sizeof(key_map_list) / sizeof(key_map); i++)
        {
            if ((change = state.Gamepad.wButtons ^ previous_state.Gamepad.wButtons) > 0)
            {
                if ((change & key_map_list[i].gamepad_button) == key_map_list[i].gamepad_button) {
                    if ((previous_state.Gamepad.wButtons & key_map_list[i].gamepad_button) == 0) {
                        if (key_map_list[i].type == MOUSE)
                        {
                            input.mi.dwFlags = input.mi.dwFlags | key_map_list[i].mouse_button_down;
                        }
                        else {
                            CreateThread(NULL, 0, mpc_be_control_send, (LPVOID)key_map_list[i].mpc_be_control, 0, NULL);
                            //mpc_be_control_send(key_map_list[i].mpc_be_control);
                        }
                    }
                    else
                    {
                        if (key_map_list[i].type == MOUSE) {
                            input.mi.dwFlags = input.mi.dwFlags | key_map_list[i].mouse_button_up;
                        }
                    }
                }
            }
        }
        if (state.Gamepad.sThumbRY * state.Gamepad.sThumbRY / 16572416 > 0)
        {
            input.mi.dwFlags = input.mi.dwFlags | MOUSEEVENTF_WHEEL;
            input.mi.mouseData = state.Gamepad.sThumbRY / 4096;
        }
        SendInput(1, &input, sizeof(INPUT));
        Sleep(3);
        previous_state = state;
        ZeroMemory(&input, sizeof(INPUT));
    }
}

DWORD check_connection() {
    DWORD dwResult;
    for (DWORD i = 0; i < XUSER_MAX_COUNT; i++)
    {
        XINPUT_STATE state;
        ZeroMemory(&state, sizeof(XINPUT_STATE));

        dwResult = XInputGetState(i, &state);

        if (dwResult == ERROR_SUCCESS)
        {
            return i;
        }
    }
    return XUSER_MAX_COUNT + 1;
}

int main(void)
{
    DWORD UserIndex = 0;
    while (true)
    {
        UserIndex = check_connection();
        if (UserIndex != 5)
        {
            break;
        }
        Sleep(100);
    }

    mouse(UserIndex);
}