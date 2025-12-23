#include <windows.h>
#include <wtsapi32.h>
#include <winternl.h>

#define CMD_ON  1001
#define CMD_OFF 1002

#define SETTINGS_KEY L"SOFTWARE\\Valve\\Steam\\NoSteamWebHelper"
#define SETTINGS_VAL L"ForceOn"

static volatile BOOL _state_on = FALSE;
static volatile HANDLE _hThread = NULL;

static DWORD WINAPI ThreadProc(LPVOID lpParameter);

static void SaveState(BOOL state)
{
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, SETTINGS_KEY, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
    {
        RegSetValueExW(hKey, SETTINGS_VAL, 0, REG_DWORD, (const BYTE*)&state, sizeof(state));
        RegCloseKey(hKey);
    }
}

static void LoadState(void)
{
    HKEY hKey;
    DWORD data = 0;
    DWORD size = sizeof(data);
    if (RegOpenKeyExW(HKEY_CURRENT_USER, SETTINGS_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        if (RegQueryValueExW(hKey, SETTINGS_VAL, NULL, NULL, (LPBYTE)&data, &size) == ERROR_SUCCESS)
        {
            _state_on = (BOOL)data;
        }
        RegCloseKey(hKey);
    }
}

static BOOL IsGameRunning(void)
{
    HKEY hKey = NULL;
    BOOL isRunning = FALSE;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Valve\\Steam", REG_OPTION_NON_VOLATILE, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS)
    {
        RegGetValueW(hKey, NULL, L"RunningAppID", RRF_RT_REG_DWORD, NULL, (PVOID)&isRunning, &((DWORD){sizeof(BOOL)}));
        RegCloseKey(hKey);
    }
    return isRunning;
}

static VOID KillSteamHelpers(void)
{
    WTS_PROCESS_INFOW *pProcessInfo = NULL;
    DWORD count = 0;
    
    if (WTSEnumerateProcessesW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pProcessInfo, &count))
    {
        for (DWORD i = 0; i < count; i++)
        {
            if (CompareStringOrdinal(pProcessInfo[i].pProcessName, -1, L"steamwebhelper.exe", -1, TRUE) == CSTR_EQUAL)
            {
                HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE, FALSE, pProcessInfo[i].ProcessId);
                if (hProcess)
                {
                    PROCESS_BASIC_INFORMATION pbi = {}; 
                    if (NT_SUCCESS(NtQueryInformationProcess(hProcess, ProcessBasicInformation, &pbi, sizeof(PROCESS_BASIC_INFORMATION), NULL)))
                    {
                        if (pbi.InheritedFromUniqueProcessId == GetCurrentProcessId())
                        {
                            TerminateProcess(hProcess, EXIT_SUCCESS);
                        }
                    }
                    CloseHandle(hProcess);
                }
            }
        }
        WTSFreeMemory(pProcessInfo);
    }
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static NOTIFYICONDATAW nid = {.cbSize = sizeof(NOTIFYICONDATAW),
                                .uCallbackMessage = WM_USER,
                                .uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP,
                                .szTip = L"Steam WebHelper"};
    static UINT wmTaskbarCreated = WM_NULL;

    switch (uMsg)
    {
    case WM_CREATE:
        wmTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
        nid.hWnd = hWnd;
        nid.hIcon = LoadIconW(NULL, IDI_APPLICATION);
        Shell_NotifyIconW(NIM_ADD, &nid);
        break;

    case WM_USER:
        if (lParam == WM_RBUTTONDOWN)
        {
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING | (_state_on ? MF_CHECKED : MF_UNCHECKED), CMD_ON,  L"Steam on");
            AppendMenuW(hMenu, MF_STRING | (_state_on ? MF_UNCHECKED : MF_CHECKED), CMD_OFF, L"Steam off");
            
            SetForegroundWindow(hWnd);
            POINT pt = {};
            GetCursorPos(&pt);
            
            UINT cmd = TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_RETURNCMD,
                                      pt.x, pt.y, 0, hWnd, NULL);

            if (cmd == CMD_ON)
            {
                _state_on = TRUE;
                SaveState(TRUE);
                if (_hThread) ResumeThread(_hThread);
            }
            else if (cmd == CMD_OFF)
            {
                _state_on = FALSE;
                SaveState(FALSE);
                if (IsGameRunning())
                {
                    if (_hThread) SuspendThread(_hThread);
                    KillSteamHelpers();
                }
            }
            DestroyMenu(hMenu);
        }
        break;

    default:
        if (uMsg == wmTaskbarCreated)
            Shell_NotifyIconW(NIM_ADD, &nid);
        break;
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

static VOID CALLBACK WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd, LONG idObject, LONG idChild,
                                  DWORD dwEventThread, DWORD dwmsEventTime)
{
    WCHAR szClassName[16] = {};
    GetClassNameW(hwnd, szClassName, 16);

    if (CompareStringOrdinal(L"vguiPopupWindow", -1, szClassName, -1, FALSE) != CSTR_EQUAL ||
        GetWindowTextLengthW(hwnd) < 1)
        return;

    CloseHandle(CreateThread(NULL, 0, ThreadProc, (LPVOID)FALSE, 0, NULL));

    HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, dwEventThread);
    _hThread = hThread;

    HKEY hKey = NULL;
    RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Valve\\Steam", REG_OPTION_NON_VOLATILE, KEY_NOTIFY | KEY_QUERY_VALUE, &hKey);

    while (!RegNotifyChangeKeyValue(hKey, FALSE, REG_NOTIFY_CHANGE_LAST_SET, NULL, FALSE))
    {
        BOOL isGameRunning = FALSE;
        RegGetValueW(hKey, NULL, L"RunningAppID", RRF_RT_REG_DWORD, NULL, (PVOID)&isGameRunning, &((DWORD){sizeof(BOOL)}));

        if (isGameRunning)
        {
            if (_state_on) 
            {
                ResumeThread(hThread);
            }
            else 
            {
                SuspendThread(hThread);
                KillSteamHelpers();
            }
        }
        else
        {
            ResumeThread(hThread);
        }
    }
}

static DWORD WINAPI ThreadProc(LPVOID lpParameter)
{
    if (lpParameter)
        SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_CREATE, NULL, WinEventProc, GetCurrentProcessId(), 0, WINEVENT_OUTOFCONTEXT);
    else
        CreateWindowExW(
            WS_EX_LEFT | WS_EX_LTRREADING,
            (LPCWSTR)(ULONG_PTR)RegisterClassW(&((WNDCLASSW){
                .lpszClassName = L" ", .hInstance = GetModuleHandleW(NULL), .lpfnWndProc = (WNDPROC)WndProc})),
            NULL, WS_OVERLAPPED, 0, 0, 0, 0, NULL, NULL, GetModuleHandleW(NULL), NULL);

    MSG msg = {};
    while (GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return EXIT_SUCCESS;
}

BOOL WINAPI DllMainCRTStartup(HINSTANCE hLibModule, DWORD dwReason, LPVOID lpReserved)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        LoadState();
        DisableThreadLibraryCalls(hLibModule);
        CloseHandle(CreateThread(NULL, 0, ThreadProc, (LPVOID)TRUE, 0, NULL));
    }
    return TRUE;
}
