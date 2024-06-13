#include <windows.h>
#include <thread>
#include <chrono>
#include <string>
#include <mutex>
#include <condition_variable>

#define IDC_START_BUTTON 101
#define IDC_LISTBOX1 102
#define IDC_LISTBOX2 103

std::mutex mtx;
std::mutex buttonMtx;
std::condition_variable cv;
int currentValue = 0;
bool ready = false;
HWND startButton;

LRESULT CALLBACK WindowProcedure(HWND, UINT, WPARAM, LPARAM);

void reenableButton(HWND hwnd) {
    std::lock_guard<std::mutex> lock(buttonMtx);
    EnableWindow(startButton, TRUE); // Re-enable the Start button
}

void printNumbersTable1(HWND hwnd) {
    {
        std::lock_guard<std::mutex> lock(mtx);
        SendDlgItemMessage(hwnd, IDC_LISTBOX1, LB_RESETCONTENT, 0, 0); // Clear the listbox
    }

    for (int i = 1; i <= 100; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1)); // Wait for 1 second

        {
            std::lock_guard<std::mutex> lock(mtx); // Lock the mutex to ensure synchronized output
            currentValue = i;
            ready = true;
            cv.notify_one(); // Notify the second thread
            std::wstring str = std::to_wstring(i);

            // Add the number to ListBox1
            SendDlgItemMessage(hwnd, IDC_LISTBOX1, LB_ADDSTRING, 0, (LPARAM)str.c_str());
            SendDlgItemMessage(hwnd, IDC_LISTBOX1, LB_SETTOPINDEX, SendDlgItemMessage(hwnd, IDC_LISTBOX1, LB_GETCOUNT, 0, 0) - 1, 0);

            // If the number is a multiple of 5, add it again to ListBox1 and once to ListBox2
            if (i % 5 == 0) {
                SendDlgItemMessage(hwnd, IDC_LISTBOX1, LB_ADDSTRING, 0, (LPARAM)str.c_str());
                SendDlgItemMessage(hwnd, IDC_LISTBOX1, LB_SETTOPINDEX, SendDlgItemMessage(hwnd, IDC_LISTBOX1, LB_GETCOUNT, 0, 0) - 1, 0);
                SendDlgItemMessage(hwnd, IDC_LISTBOX2, LB_ADDSTRING, 0, (LPARAM)str.c_str());
                SendDlgItemMessage(hwnd, IDC_LISTBOX2, LB_SETTOPINDEX, SendDlgItemMessage(hwnd, IDC_LISTBOX2, LB_GETCOUNT, 0, 0) - 1, 0);
            }
        }
    }
    reenableButton(hwnd);
}

void printNumbersTable2(HWND hwnd) {
    {
        std::lock_guard<std::mutex> lock(mtx);
        SendDlgItemMessage(hwnd, IDC_LISTBOX2, LB_RESETCONTENT, 0, 0); // Clear the listbox
    }

    for (int i = 1; i <= 20; ++i) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [] { return ready && currentValue % 5 == 0; }); // Wait until a multiple of 5 is available

        // Just acknowledge that the number has been processed
        ready = false;
    }
    reenableButton(hwnd);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR args, int ncmdshow) {
    WNDCLASSW wc = { 0 };

    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hInstance = hInst;
    wc.lpszClassName = L"myWindowClass";
    wc.lpfnWndProc = WindowProcedure;

    if (!RegisterClassW(&wc))
        return -1;

    CreateWindowW(L"myWindowClass", L"Multithreaded GUI", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        100, 100, 600, 600, NULL, NULL, hInst, NULL); // Adjusted window size

    MSG msg = { 0 };

    while (GetMessage(&msg, NULL, NULL, NULL)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        startButton = CreateWindowW(L"Button", L"Start", WS_VISIBLE | WS_CHILD,
            250, 20, 80, 25, hwnd, (HMENU)IDC_START_BUTTON, NULL, NULL);
        CreateWindowW(L"ListBox", NULL, WS_VISIBLE | WS_CHILD | WS_BORDER | LBS_NOTIFY | WS_VSCROLL,
            50, 60, 200, 450, hwnd, (HMENU)IDC_LISTBOX1, NULL, NULL); // Adjusted height and added WS_VSCROLL
        CreateWindowW(L"ListBox", NULL, WS_VISIBLE | WS_CHILD | WS_BORDER | LBS_NOTIFY | WS_VSCROLL,
            350, 60, 200, 450, hwnd, (HMENU)IDC_LISTBOX2, NULL, NULL); // Adjusted height and added WS_VSCROLL
        break;
    case WM_COMMAND:
        if (LOWORD(wp) == IDC_START_BUTTON) {
            EnableWindow(startButton, FALSE); // Disable the Start button
            std::thread t1(printNumbersTable1, hwnd);
            std::thread t2(printNumbersTable2, hwnd);
            t1.detach();
            t2.detach();
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    return 0;
}
