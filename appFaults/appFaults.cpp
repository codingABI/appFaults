
/*+===================================================================
  File:      appFaults.cpp

  Summary:   Test/stress tool that creates typical app faults, like a memory leak, freezes ...
             The tool can be used to show or debug the effects of programming errors
             Use at your own risk!

             Program should run on Windows 11/10/8.1/2022/2019/2016/2012R2

  License: CC0
  Copyright (c) 2024 codingABI

  Icon for the app: Modified icon "gear" from Game icon pack by Kenney Vleugels (www.kenney.nl), https://kenney.nl/assets/game-icons, CC0

  History:
  20240810, Initial version
  20240813, Remove unneeded spaces, Change manifest for dpi PerMonitorV2

===================================================================+*/

#include "framework.h"
#include "appFaults.h"
#include <string>
#include <commctrl.h>
#include <shellapi.h>
#include <process.h>
#include <ShellScalingApi.h>

// Add libs (for Visual Studio)
#pragma comment(lib,"comctl32.lib")
#pragma comment(lib,"shcore.lib")
#pragma comment(lib,"Version.lib")

// Default sizes for some control in 96 dpi
#define WINDOWWIDTH_96DPI 400
#define WINDOWHEIGHT_96DPI 480

// Struct for an automatically generated button
typedef struct {
    PVOID  dwResourceID; // Resource ID for button
    DWORD dwResourceStringID; // Resource string as button text
} AUTOBUTTON;

// List of automatically generated buttons
#define MAXAUTOBUTTONS 9
AUTOBUTTON g_autoButtons[MAXAUTOBUTTONS] = {
    { (PVOID) IDM_LOOP,IDS_LOOP },
    { (PVOID) IDM_LOOPTHREAD,IDS_LOOPTHREAD},
    { (PVOID) IDM_LOCK10S,IDS_LOCK10S },
    { (PVOID) IDM_MEMORYLEAK,IDS_MEMORYLEAK },
    { (PVOID) IDM_HANDLELEAK,IDS_HANDLELEAK },
    { (PVOID) IDM_GDILEAK,IDS_GDILEAK },
    { (PVOID) IDM_THREADSPAM,IDS_THREADSPAM },
    { (PVOID) IDM_FREEINVALID,IDS_FREEINVALID },
    { (PVOID) IDM_NULLACCESS,IDS_NULLACCESS}
};

// Global variables
HINSTANCE g_hInst;
int g_iFontHeight_96DPI = -12;
HFONT g_hFont = NULL;
HWND g_hStatusBar = NULL;
HANDLE g_semaphore = NULL;
HWND g_hLastFocus = NULL;
HWND g_hWnd = NULL;

// Function declarations
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);

/*
 * I had a problem with the built in GetDpiForWindow function.
 * When I use this function and run the program on Windows 2012R2 the program does not start and I will get the message
 * 'Der Prozedureinsprungpunkt "GetDpiForWindow" wurde in der DLL "...exe" nicht gefunden. '
 * a.k.a. 'The procedure entry point "GetDpiForWindow" could not be located in the dynamic link library "...exe"'
 * My workaround is MyGetDpiForWindow
 */
UINT MyGetDpiForWindow(HWND hWindow) {
    UINT uDpi = 96; // Failback value, if GetDpiForWindow could not be used

    HMODULE hDLL = GetModuleHandle(L"user32.dll");
    if (hDLL == NULL)
        return uDpi;

    UINT(__stdcall * GetDpiForWindow)(HWND hwnd);
    *reinterpret_cast<FARPROC*>(&GetDpiForWindow) = GetProcAddress(hDLL, "GetDpiForWindow");

    if (GetDpiForWindow == nullptr)
        return uDpi;
    return(GetDpiForWindow(hWindow));
}

/*F+F+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  Function: LoadStringAsWstr

  Summary:  Get resource string as wstring

  Args:     HINSTANCE hInstance
              Handle to instance module
            UINT uID
              Resource ID

  Returns:  std::wstring

-----------------------------------------------------------------F-F*/
std::wstring LoadStringAsWstr(HINSTANCE hInstance, UINT uID) {
    PCWSTR pws;
    int cchStringLength = LoadStringW(hInstance, uID, reinterpret_cast<LPWSTR>(&pws), 0);
    if (cchStringLength > 0) return std::wstring(pws, cchStringLength); else return std::wstring();
}

/*F+F+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  Function: showProgramInformation

  Summary:   Show program information message box

  Args:     HWND hWindow
              Handle to main window

  Returns:

-----------------------------------------------------------------F-F*/
void showProgramInformation(HWND hWindow) {
    std::wstring sTitle(LoadStringAsWstr(g_hInst, IDS_APP_TITLE));
    wchar_t szExecutable[MAX_PATH];
    GetModuleFileName(NULL, szExecutable, MAX_PATH);

    DWORD  verHandle = 0;
    UINT   size = 0;
    LPBYTE lpBuffer = NULL;
    DWORD  verSize = GetFileVersionInfoSize(szExecutable, NULL);

    if (verSize != 0) {
        BYTE* verData = new BYTE[verSize];

        if (GetFileVersionInfo(szExecutable, verHandle, verSize, verData))
        {
            if (VerQueryValue(verData, L"\\", (VOID FAR * FAR*) & lpBuffer, &size)) {
                if (size) {
                    VS_FIXEDFILEINFO* verInfo = (VS_FIXEDFILEINFO*)lpBuffer;
                    if (verInfo->dwSignature == 0xfeef04bd) {
                        sTitle.append(L" ")
                            .append(std::to_wstring((verInfo->dwFileVersionMS >> 16) & 0xffff))
                            .append(L".")
                            .append(std::to_wstring((verInfo->dwFileVersionMS >> 0) & 0xffff))
                            .append(L".")
                            .append(std::to_wstring((verInfo->dwFileVersionLS >> 16) & 0xffff))
                            .append(L".")
                            .append(std::to_wstring((verInfo->dwFileVersionLS >> 0) & 0xffff));
                    }
                }
            }
        }
        delete[] verData;
    }

    MessageBox(hWindow, LoadStringAsWstr(g_hInst, IDS_PROGINFO).c_str(), sTitle.c_str(), MB_ICONINFORMATION | MB_OK);
}


/*F+F+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  Function: threadLoop

  Summary:   Faulty thread functions, that runs an endless loop

  Args:     void* data
              Unused

  Returns:  unsigned int
              0, but never returns

-----------------------------------------------------------------F-F*/
unsigned int __stdcall threadLoop(void* data) {
    while (true);
    return 0;
}

/*F+F+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  Function: threadWaitForever

  Summary:   Faulty thread functions, that waits forever 

  Args:     void* data
              Unused

  Returns:  unsigned int
              0, but never returns

-----------------------------------------------------------------F-F*/
unsigned int __stdcall threadWaitForever(void* data) {
    WaitForSingleObject(g_semaphore, INFINITE);
    return 0;
}

/*F+F+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  Function: resizeWindow

  Summary:   Resize/reposition main window

  Args:     HWND hWindow
              Handle to main window
            RECT* prcNewWindow
              Pointer to RECT structur to new size and position
              NULL = Resize to initial window size

  Returns:

-----------------------------------------------------------------F-F*/
void resizeWindow(HWND hWindow, RECT* prcNewWindow) {
    UINT uDpi = MyGetDpiForWindow(hWindow);

    if (hWindow == NULL) return;

    if (prcNewWindow != NULL) { // Set requested size
        SetWindowPos(hWindow, NULL, prcNewWindow->left, prcNewWindow->top, prcNewWindow->right - prcNewWindow->left, prcNewWindow->bottom - prcNewWindow->top, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    else {
        // Set initial window size
        int iWidth = MulDiv(WINDOWWIDTH_96DPI, uDpi, USER_DEFAULT_SCREEN_DPI);
        int iHeight = MulDiv(WINDOWHEIGHT_96DPI, uDpi, USER_DEFAULT_SCREEN_DPI);

        SetWindowPos(hWindow, NULL, 0, 0, iWidth, iHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

/*F+F+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  Function: resizeControls

  Summary:   Resize/reposition window controls

  Args:     HWND hWindow
              Handle to main window

  Returns:

-----------------------------------------------------------------F-F*/
void resizeControls(HWND hWindow) {
    UINT uDpi = MyGetDpiForWindow(hWindow);
    LONG units = GetDialogBaseUnits();

    // Check handles
    if (hWindow == NULL) return;
    if (g_hStatusBar == NULL) return;

    // Get height/width of window inner area
    RECT rectWindow{ 0,0,0,0 };
    GetClientRect(hWindow, &rectWindow);
    int iClientWidth = rectWindow.right;
    int iClientHeight = rectWindow.bottom;

    // Set icons for window caption (aligned on https://learn.microsoft.com/en-us/previous-versions/windows/desktop/mpc/creating-a-dpi-aware-application)
    if (uDpi >= 192) {
        SendMessage(hWindow, WM_SETICON, ICON_BIG, (LONG)(ULONG_PTR)LoadImage(g_hInst, MAKEINTRESOURCE(IDI_APPICON64), IMAGE_ICON, 64, 64, LR_DEFAULTCOLOR | LR_SHARED));
        SendMessage(hWindow, WM_SETICON, ICON_SMALL, (LONG)(ULONG_PTR)LoadImage(g_hInst, MAKEINTRESOURCE(IDI_APPICON32), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR | LR_SHARED));
    } else if (uDpi >= 144) {
        SendMessage(hWindow, WM_SETICON, ICON_BIG, (LONG)(ULONG_PTR)LoadImage(g_hInst, MAKEINTRESOURCE(IDI_APPICON48), IMAGE_ICON, 48, 48, LR_DEFAULTCOLOR | LR_SHARED));
        SendMessage(hWindow, WM_SETICON, ICON_SMALL, (LONG)(ULONG_PTR)LoadImage(g_hInst, MAKEINTRESOURCE(IDI_APPICON32), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR | LR_SHARED));
    } else {
        SendMessage(hWindow, WM_SETICON, ICON_BIG, (LONG)(ULONG_PTR)LoadImage(g_hInst, MAKEINTRESOURCE(IDI_APPICON32), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR | LR_SHARED));
        SendMessage(hWindow, WM_SETICON, ICON_SMALL, (LONG)(ULONG_PTR)LoadImage(g_hInst, MAKEINTRESOURCE(IDI_APPICON16), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR | LR_SHARED));
    }

    // Update automatic size of status bar
    SendMessage(g_hStatusBar, WM_SIZE, 0, 0);

    // Sets font size for controls from menu font size
    NONCLIENTMETRICS ncm;
    ncm.cbSize = sizeof(NONCLIENTMETRICS);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0);

    // Scale font size to x1.5 (I am getting old...)
    ncm.lfMenuFont.lfHeight = 3 * MulDiv(g_iFontHeight_96DPI, uDpi, USER_DEFAULT_SCREEN_DPI) / 2;

    if (g_hFont != NULL) DeleteObject(g_hFont); // Prevents GDI leak
    g_hFont = CreateFontIndirect(&ncm.lfMenuFont);

    // Set font for status bar
    SendMessage(g_hStatusBar, WM_SETFONT, (WPARAM)g_hFont, MAKELPARAM(TRUE, 0));

    // Get status bar size
    RECT rectStatusBar{ 0,0,0,0 };
    GetClientRect(g_hStatusBar, &rectStatusBar);
    int iStatusBarHeight = rectStatusBar.bottom;

    // Resize automatically generated buttons
    int iButtonHeight_96DPI = MulDiv(HIWORD(units), 14, 8);
    int iMargin = ((iClientHeight - iStatusBarHeight) 
        - MAXAUTOBUTTONS * MulDiv(iButtonHeight_96DPI, uDpi, USER_DEFAULT_SCREEN_DPI)) / (MAXAUTOBUTTONS + 1);
    int iPosY = iMargin;
    for (int i = 0; i < MAXAUTOBUTTONS; i++) {
        HWND hButton = GetDlgItem(hWindow, IDM_LOOP + i);
        if (hButton != NULL) {
            // Set font for button
            SendMessage(hButton, WM_SETFONT, (WPARAM)g_hFont, MAKELPARAM(TRUE, 0));
            // Set size for button
            SetWindowPos(hButton, 0,
                iMargin,
                iPosY,
                iClientWidth - iMargin * 2,
                MulDiv(iButtonHeight_96DPI, uDpi, USER_DEFAULT_SCREEN_DPI) ,
                SWP_NOZORDER);
            iPosY += iMargin + MulDiv(iButtonHeight_96DPI, uDpi, USER_DEFAULT_SCREEN_DPI);
        }
    }
    
    InvalidateRect(hWindow, NULL, TRUE); // Force window content update
}

/*F+F+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  Function: createControls

  Summary:   Add all controls to window

  Args:     HWND hWindow
              Handle to main window

  Returns:

-----------------------------------------------------------------F-F*/
void createControls(HWND hWindow)
{
    UINT uDpi = MyGetDpiForWindow(hWindow);
    std::wstring sResource;

    // Remember font size for menu. Will be needed in resize function
    NONCLIENTMETRICS ncm;
    ncm.cbSize = sizeof(NONCLIENTMETRICS);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0);
    g_iFontHeight_96DPI = MulDiv(ncm.lfMenuFont.lfHeight, USER_DEFAULT_SCREEN_DPI, uDpi);

    // Automatically generated buttons
    for (int i = 0; i < MAXAUTOBUTTONS; i++) {
        HWND hwndButton = CreateWindow(
            L"BUTTON",
            LoadStringAsWstr(g_hInst, g_autoButtons[i].dwResourceStringID).c_str(),
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_TEXT | BS_VCENTER,
            0, 0, 0, 0, // Size will be set later
            hWindow,
            (HMENU)(g_autoButtons[i].dwResourceID),
            g_hInst,
            NULL);
        if (hwndButton != NULL) {
            if (i == 0) { // Set focus to first button
                SetFocus(hwndButton);
                g_hLastFocus = hwndButton;
            }
        }
    }
    // Status bar
    g_hStatusBar = CreateWindow(
        STATUSCLASSNAME,
        NULL,
        WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, // Size will be set automatically
        hWindow,
        (HMENU)IDC_STATUSBAR,
        g_hInst,
        (LPVOID)NULL);

    // Sysmenu entry
    HMENU hSysMenu = GetSystemMenu(hWindow, FALSE);
    if (hSysMenu != NULL) {
        InsertMenu(hSysMenu, -1, MF_BYPOSITION, MF_SEPARATOR, NULL); // Seperator
        InsertMenu(hSysMenu, -1, MF_BYPOSITION, (UINT)IDM_ABOUT, LoadStringAsWstr(g_hInst, IDS_ABOUT).c_str()); // About
    }
}

/*F+F+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  Function: wWinMain

  Summary:   Process window messages of main window

  Args:     HINSTANCE hInstance
            HINSTANCE hPrevInstance
            LPWSTR lpCmdLine
            int nCmdShow

  Returns:  int
              0 = success
              1 = error

-----------------------------------------------------------------F-F*/
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    MyRegisterClass(hInstance);

    // Enables controls from Comctl32.dll, like status bar ...
    InitCommonControls();

    // Start task manager as a usefull tool
    ShellExecute(NULL, L"open", L"taskmgr.exe", NULL, NULL, SW_SHOWNORMAL);

    // New semaphore with inital value of 0 (used tp create hanging threads)
    g_semaphore = CreateSemaphore(NULL, 0, 1, NULL);

    // Init application
    if (!InitInstance (hInstance, nCmdShow)) return 1;

    MSG msg;

    // Message loop
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (IsDialogMessage(g_hWnd, &msg)) {

        } else {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    // Cleanup
    DeleteObject(g_hFont);
    CloseHandle(g_semaphore);

    return (int) msg.wParam;
}

/*F+F+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  Function: MyRegisterClass

  Summary:   Register window class

  Args:     HINSTANCE hInstance
              Handle to the instance that contains the window procedure

  Returns:  ATOM
              ID of registered class
              NULL = error

-----------------------------------------------------------------F-F*/
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPFAULTS)); // Icon for taskbar and explorer (icon for explorer seems to be the icon resource with the lowest ID)
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)CreateSolidBrush(RGB(255,0,0)); // Background color to red
    wcex.lpszMenuName = L"MainMenu";
    wcex.lpszClassName = L"MainWndClass";
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_APPICON16)); // Small icon for window caption

    return RegisterClassExW(&wcex);
}

/*F+F+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  Function: InitInstance

  Summary:   Stores the handle to the instance in a global variable and creates and shows the main window

  Args:     HINSTANCE hInstance
              Handle to the instance that contains the window procedure
            int nCmdShow
              Controls how the window is to be shown

  Returns:  BOOL
              TRUE = success
              FALSe = error

-----------------------------------------------------------------F-F*/
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   g_hInst = hInstance;

   g_hWnd = CreateWindowW(
       L"MainWndClass",
       LoadStringAsWstr(g_hInst, IDS_APP_TITLE).c_str(),
       WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX, 
       CW_USEDEFAULT, 
       CW_USEDEFAULT, 
       WINDOWWIDTH_96DPI, 
       WINDOWHEIGHT_96DPI, 
       nullptr, nullptr, hInstance, nullptr);

   if (!g_hWnd)
   {
      return FALSE;
   }

   ShowWindow(g_hWnd, nCmdShow);
   UpdateWindow(g_hWnd);

   return TRUE;
}

/*F+F+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  Function: WndProc

  Summary:   Process window messages of main window

  Args:     HWND hWnd
              Handle to main window
            UINT message
              Message
            WPARAM wParam
            LPARAM lParam

  Returns:  LRESULT

-----------------------------------------------------------------F-F*/
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static int iThreadCount = 0;
    char* pszTest = NULL;

    switch (message)
    {
    case WM_SYSCOMMAND:
        switch (wParam) {
            case IDM_ABOUT:
                showProgramInformation(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;
    case WM_SETFOCUS:
        if (g_hLastFocus) SetFocus(g_hLastFocus);
        break;
    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE) g_hLastFocus = GetFocus();
        break;
    case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
                case IDM_LOOP:
                    while (true); // Fault
                    break;
                case IDM_LOOPTHREAD:
                    _beginthreadex(0, 0, &threadLoop, (void*)hWnd, 0, 0); // Fault
                    iThreadCount++;
                    SendMessage(g_hStatusBar, SB_SETTEXT, 0,
                        (LPARAM)std::wstring(std::to_wstring(iThreadCount))
                        .append(L" ")
                        .append(LoadStringAsWstr(g_hInst, IDS_LOOPTHREADS)).c_str());
                    break;
                case IDM_THREADSPAM:
                    while (true) _beginthreadex(0, 0, &threadWaitForever, (void*)hWnd, 0, 0); // Fault
                    break;
                case IDM_LOCK10S:
                    Sleep(60000); // Fault
                    break;
                case IDM_MEMORYLEAK:
                    if (MessageBox(hWnd,
                        LoadStringAsWstr(g_hInst, IDS_MEMORYLEAKWARING).c_str(),
                        LoadStringAsWstr(g_hInst, IDS_MEMORYLEAK).c_str(),
                        MB_YESNO | MB_ICONQUESTION | MB_APPLMODAL) == IDYES) {
                        while (true) HWND* phWindow = (HWND*)malloc(sizeof(HWND)); // Fault
                    }
                    break;
                case IDM_NULLACCESS:
                    #pragma warning(suppress: 6011)
                    pszTest[0] = ' '; // Fault
                    break;
                case IDM_HANDLELEAK:
                    while (true) OpenProcess(PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId()); // Fault
                    break;
                case IDM_FREEINVALID:
                    pszTest = (char*)malloc(sizeof(char));
                    free(pszTest);
                    #pragma warning(suppress: 6001)
                    free(pszTest); // Fault
                    break;
                case IDM_GDILEAK:
                    while (true) CreateFont(48, 0, 0, 0, FW_DONTCARE, FALSE, TRUE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
                        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Arial"); // Fault
                    break;
                default:
                    return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_CREATE:
        createControls(hWnd);
        resizeWindow(hWnd,NULL);
        resizeControls(hWnd);
        SendMessage(g_hStatusBar, SB_SETTEXT, 0, (LPARAM)LoadStringAsWstr(g_hInst, IDS_APPWARNING).c_str());
        break;
    case WM_DPICHANGED:
        resizeWindow(hWnd,(RECT*)lParam);
        resizeControls(hWnd);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
