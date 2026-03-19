#include "Window.h"

namespace core {

Window::Window(int width, int height, const std::wstring& title)
    : m_width(width), m_height(height), m_title(title), m_hwnd(nullptr), m_isReady(false) {
    
    m_hInst = GetModuleHandle(nullptr);

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW; 
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = m_hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"HybridCoreWindowClass";

    RegisterClassExW(&wc);

    RECT rect = { 0, 0, width, height };
    // Menu eklenecegi icin TRUE gonderiyoruz ki frame boyut hesaplamasi menuye gore yapilsin
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, TRUE);

    m_hwnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr, nullptr, m_hInst, this
    );

    if (m_hwnd) {
        CreateWin32Menu(); // Menuyu olustur ve pencereye uygula
        ShowWindow(m_hwnd, SW_SHOW);
        m_isReady = true;
    }
}

Window::~Window() {
    if (m_hwnd) DestroyWindow(m_hwnd);
    UnregisterClassW(L"HybridCoreWindowClass", m_hInst);
}

void Window::CreateWin32Menu() {
    // Win32 Native Menu API kullanarak yapiyi kur
    HMENU hMenu = CreateMenu();
    HMENU hModullerMenu = CreatePopupMenu();

    // Modules Menu content
    AppendMenuW(hModullerMenu, MF_STRING, ID_MENU_HARDWARE_TEST, L"Start Continuous Heavy Simulation");
    AppendMenuW(hModullerMenu, MF_STRING, ID_MENU_STOP_TEST, L"Stop Simulation");
    AppendMenuW(hModullerMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hModullerMenu, MF_STRING, ID_MENU_EXIT, L"Exit");

    // Attach submenu to main menu
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hModullerMenu, L"Modules");

    // Modifiye edilen menuyu asil penceremize (m_hwnd) yapistir
    SetMenu(m_hwnd, hMenu);
}

bool Window::ProcessMessages() {
    MSG msg = {};
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) return false;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return m_isReady;
}

LRESULT CALLBACK Window::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_CREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pCreate->lpCreateParams);
    } else {
        Window* win = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        if (win) {
            switch (uMsg) {
                case WM_COMMAND: { 
                    // Kullanici listeden (menuden) bir elemana tikladiginda tetiklenir
                    int wmId = LOWORD(wParam);
                    // Eger Engine tarafindan fonksiyon baglanmissa (Callback), bunu tetikle
                    if (win->m_menuCallback) {
                        win->m_menuCallback(wmId);
                    }
                    return 0;
                }
                case WM_DESTROY:
                    PostQuitMessage(0);
                    win->m_isReady = false;
                    return 0;
            }
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

} // namespace core
