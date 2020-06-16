#include <iostream>
#include <curl/curl.h>
#include <xinput.h>
#include <winuser.h>
#include <stdlib.h>

#pragma comment( linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"" )

#define MOUSE 0
#define MPC_BE 1
#define BIT4 0x8
#define ALLOW_LONG_DOWN 1

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
    {MPC_BE,XINPUT_GAMEPAD_DPAD_DOWN,0,0,"wm_command=830"},
    {MPC_BE,XINPUT_GAMEPAD_DPAD_UP,0,0,"wm_command=889"},
    {MPC_BE,XINPUT_GAMEPAD_DPAD_LEFT,ALLOW_LONG_DOWN,0,"wm_command=901"},
    {MPC_BE,XINPUT_GAMEPAD_DPAD_RIGHT,ALLOW_LONG_DOWN,0,"wm_command=902"},
    {MPC_BE,XINPUT_GAMEPAD_LEFT_SHOULDER | XINPUT_GAMEPAD_X,0,0,"wm_command=921"},
    {MPC_BE,XINPUT_GAMEPAD_RIGHT_SHOULDER | XINPUT_GAMEPAD_X,0,0,"wm_command=922"},
    {MPC_BE,XINPUT_GAMEPAD_LEFT_SHOULDER | XINPUT_GAMEPAD_Y,0,0,"wm_command=919"},
    {MPC_BE,XINPUT_GAMEPAD_RIGHT_SHOULDER | XINPUT_GAMEPAD_Y,0,0,"wm_command=920"}
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

DWORD WINAPI handle(LPVOID lpParam) {

    XINPUT_STATE state = { 0 };
    XINPUT_STATE previous_state = state;
    INPUT input = { 0 };
    DWORD UserIndex = (DWORD)lpParam;
    DWORD down_time = 0;

    while (true)
    {
        if (status) {
            return 0;
        }
        if (XInputGetState(UserIndex, &state)) {
            return -1;
        }
        if ((state.Gamepad.wButtons ^ (XINPUT_GAMEPAD_LEFT_THUMB |
                                       XINPUT_GAMEPAD_RIGHT_THUMB)) == 0) {
            Sleep(2000);
            XInputGetState(UserIndex, &state);
            if ((state.Gamepad.wButtons ^ (XINPUT_GAMEPAD_LEFT_THUMB |
                                           XINPUT_GAMEPAD_RIGHT_THUMB)) == 0) {
                return 1;
            }
        }

        input.type = INPUT_MOUSE;
        input.mi.dwFlags = 0;
        input.mi.dx = state.Gamepad.sThumbLX / 4096;
        input.mi.dy = -state.Gamepad.sThumbLY / 4096;

        if (abs(input.mi.dx) + abs(input.mi.dy) > 0)
        {
            input.mi.dwFlags = input.mi.dwFlags | MOUSEEVENTF_MOVE;
        }

        if (abs(state.Gamepad.sThumbRY) / 4096 > 0)
        {
            input.mi.dwFlags = input.mi.dwFlags | MOUSEEVENTF_WHEEL;
            input.mi.mouseData = state.Gamepad.sThumbRY / 4096;
        }

        for (int i = 0; i < sizeof(key_map_list) / sizeof(key_map); i++)
        {
            switch (key_map_list[i].type)
            {
            case MOUSE:
                if ((previous_state.Gamepad.wButtons ^ key_map_list[i].gamepad_button) != 0) {
                    if ((state.Gamepad.wButtons ^ key_map_list[i].gamepad_button) == 0) {
                        input.mi.dwFlags = input.mi.dwFlags | key_map_list[i].mouse_button_down;
                    }
                }
                else
                {
                    if ((state.Gamepad.wButtons ^ key_map_list[i].gamepad_button) != 0) {
                        input.mi.dwFlags = input.mi.dwFlags | key_map_list[i].mouse_button_up;
                    }
                }
                break;
            case MPC_BE:
                if ((previous_state.Gamepad.wButtons ^ key_map_list[i].gamepad_button) != 0) {
                    if ((state.Gamepad.wButtons ^ key_map_list[i].gamepad_button) == 0) {
                        CloseHandle(CreateThread(NULL, 0, mpc_be_control_send,
                                                 (LPVOID)key_map_list[i].mpc_be_control, 0, NULL));
                    }
                }
                else
                {
                    if ((state.Gamepad.wButtons ^ key_map_list[i].gamepad_button) == 0) {
                        if (key_map_list[i].mouse_button_down)
                        {
                            down_time += 3;
                            if (down_time > 150)
                            {
                                CloseHandle(CreateThread(NULL, 0, mpc_be_control_send,
                                                         (LPVOID)key_map_list[i].mpc_be_control, 0, NULL));
                                down_time = 0;
                            }
                        }
                    }
                    else {
                        down_time = 0;
                    }
                }
                break;
            default:
                break;
            }

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
                mouse_HANDLE[i] = CreateThread(NULL, 0, handle, (LPVOID)i, 0, NULL);
                examination_HANDLE[i] = CreateThread(NULL, 0, examination, (LPVOID)i, 0, NULL);
            }
            else {
                if (mouse_HANDLE[i] != NULL)
                {
                    GetExitCodeThread(mouse_HANDLE[i], &ExitCode);
                    switch (ExitCode)
                    {
                    case 1:
                        return 0;
                    case -1:
                        CloseHandle(mouse_HANDLE[i]);
                        mouse_HANDLE[i] = NULL;
                    default:
                        break;
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