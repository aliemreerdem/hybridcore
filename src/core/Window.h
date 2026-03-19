#pragma once
#include <windows.h>
#include <string>
#include <functional>

namespace core {

// Gelecekteki moduller ve fonksiyonlar icin Menu ID'leri
constexpr int ID_MENU_HARDWARE_TEST = 1001;
constexpr int ID_MENU_STOP_TEST = 1003;
constexpr int ID_MENU_EXIT = 1002;

class Window {
public:
    Window(int width, int height, const std::wstring& title);
    ~Window();

    bool ProcessMessages();
    
    HWND GetHWND() const { return m_hwnd; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }

    // Motordan (Engine) menuye tiklanmalari dinlemek icin callback (Event Listener)
    void SetMenuCallback(std::function<void(int)> callback) { m_menuCallback = callback; }

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void CreateWin32Menu(); // Menuyu olusturmak icin fonksiyonumuz

    HWND m_hwnd;
    HINSTANCE m_hInst;
    int m_width;
    int m_height;
    std::wstring m_title;
    bool m_isReady;

    std::function<void(int)> m_menuCallback;
};

} // namespace core
