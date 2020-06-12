#include <iostream>
#include <curl/curl.h>
#include <xinput.h>
#include <winuser.h>

#pragma comment( linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"" )

#define MOUSE 0
#define MPC_BE 1
#define BIT4 0x8

DWORD status = 0;

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

    const char *fromdata = (const char *)lpParam;
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

DWORD WINAPI mouse(LPVOID lpParam) {

    XINPUT_STATE state = { 0 };
    XINPUT_STATE previous_state = state;
    WORD change = 0;
    INPUT input = { 0 };
    DWORD UserIndex = (DWORD)lpParam;

    while (true)
    {
        if (status) {
            return 0;
        }
        if (XInputGetState(UserIndex, &state)) {
            return -1;
        }
        input.type = INPUT_MOUSE;
        input.mi.dx = state.Gamepad.sThumbLX / 4096;
        input.mi.dy = -state.Gamepad.sThumbLY / 4096;
        input.mi.dwFlags = MOUSEEVENTF_MOVE;

        if ((change = state.Gamepad.wButtons ^ previous_state.Gamepad.wButtons))
        {
            for (int i = 0; i < sizeof(key_map_list) / sizeof(key_map); i++)
            {
                if ((change & key_map_list[i].gamepad_button) == key_map_list[i].gamepad_button) {
                    if ((previous_state.Gamepad.wButtons & key_map_list[i].gamepad_button) == 0) {
                        if (key_map_list[i].type == MOUSE)
                        {
                            input.mi.dwFlags = input.mi.dwFlags | key_map_list[i].mouse_button_down;
                        }
                        else {
                            CloseHandle(CreateThread(NULL, 0, mpc_be_control_send,
                                                     (LPVOID)key_map_list[i].mpc_be_control, 0, NULL));
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
        if (state.Gamepad.sThumbRY * state.Gamepad.sThumbRY / 16777216 > 1)
        {
            input.mi.dwFlags = input.mi.dwFlags | MOUSEEVENTF_WHEEL;
            input.mi.mouseData = state.Gamepad.sThumbRY / 4096;
        }
        SendInput(1, &input, sizeof(INPUT));
        Sleep(3);
        previous_state = state;
    }
}

DWORD WINAPI examination(LPVOID lpParam) {

    XINPUT_STATE state = { 0 };
    DWORD UserIndex = (DWORD)lpParam;
    DWORD exit_status = 0;
    if (UserIndex & BIT4) {
        UserIndex = UserIndex - BIT4;
        exit_status = 1;
    }
    if (status) {
        Sleep(5000);
    }

    while (true)
    {
        if (XInputGetState(UserIndex, &state)) {
            return -1;
        }
        if ((state.Gamepad.wButtons ^ (XINPUT_GAMEPAD_DPAD_DOWN |
                                       XINPUT_GAMEPAD_LEFT_SHOULDER |
                                       XINPUT_GAMEPAD_RIGHT_SHOULDER)) == 0) {
            Sleep(2000); 
            XInputGetState(UserIndex, &state);
            if ((state.Gamepad.wButtons ^ (XINPUT_GAMEPAD_DPAD_DOWN |
                                           XINPUT_GAMEPAD_LEFT_SHOULDER |
                                           XINPUT_GAMEPAD_RIGHT_SHOULDER)) == 0) {
                return exit_status;
            }
        }
        Sleep(50);
    }
}


int main(void)
{
    DWORD UserIndex = 0;
    XINPUT_STATE state = { 0 };
    HANDLE mouse_HANDLE[4] = { NULL };
    HANDLE examination_HANDLE[4] = { NULL };
    DWORD ExitCode = 0;


    while (true)
    {
        DWORD dwResult;
        for (DWORD i = 0; i < XUSER_MAX_COUNT; i++)
        {
            dwResult = XInputGetState(i, &state);

            if (dwResult == ERROR_SUCCESS && mouse_HANDLE[i] == NULL)
            {
                status = 0;
                mouse_HANDLE[i] = CreateThread(NULL, 0, mouse, (LPVOID)i, 0, NULL);
                examination_HANDLE[i] = CreateThread(NULL, 0, examination, (LPVOID)i, 0, NULL);
            }
            else {
                if (mouse_HANDLE[i] != NULL)
                {
                    GetExitCodeThread(mouse_HANDLE[i], &ExitCode);
                    if (ExitCode == -1) {
                        CloseHandle(mouse_HANDLE[i]);
                        mouse_HANDLE[i] = NULL;
                    }
                    GetExitCodeThread(examination_HANDLE[i], &ExitCode);
                    switch (ExitCode)
                    {
                    case 1:
                        CloseHandle(mouse_HANDLE[i]);
                        mouse_HANDLE[i] = NULL;
                        break;
                    case 0:
                        status = 1;
                        CloseHandle(examination_HANDLE[i]); 
                        examination_HANDLE[i] = CreateThread(NULL, 0, examination, (LPVOID)(i + BIT4), 0, NULL);
                        break;
                    case -1:
                        CloseHandle(examination_HANDLE[i]);
                        examination_HANDLE[i] = NULL;
                        break;
                    default:
                        break;
                    }
                }
            }
        }
        Sleep(1000);
    }
}