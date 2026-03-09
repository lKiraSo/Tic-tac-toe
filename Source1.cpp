#include <windows.h>
#include <ctime>

char board[9];        // Поле: 'X', 'O' або ' ' (пробіл)
HWND buttons[9];      // Масив самих кнопок
bool gameOver = false; // Чи закінчилась гра

// Оголошення функцій
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void BotMove();

// Головна функція, яка запускає вікно
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR pCmd, int nShow) {
    srand(time(0)); // Щоб бот завжди ходив по-різному

    // Очмщення поля перед початком
    for (int i = 0; i < 9; i++) board[i] = ' ';

    const wchar_t CLASS_NAME[] = L"GameClass";
    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInst;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClass(&wc);

    // Створюємо вікно
    HWND hwnd = CreateWindowEx(0, CLASS_NAME, L"Tic Tac Toe", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 316, 450, NULL, NULL, hInst, NULL);

    ShowWindow(hwnd, nShow);

    // Очікування дій від користувача
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

// Перевірка, чи хтось виграв
bool CheckWinner(char p) {
    int w[8][3] = { {0,1,2}, {3,4,5}, {6,7,8}, {0,3,6}, {1,4,7}, {2,5,8}, {0,4,8}, {2,4,6} };
    for (int i = 0; i < 8; i++) {
        if (board[w[i][0]] == p && board[w[i][1]] == p && board[w[i][2]] == p) return true;
    }
    return false;
}

// Хід бота
void BotMove() {
    if (gameOver) return;

    int choice;
    bool found = false;

    // Перевіряємо, чи є вільні місця
    for (int i = 0; i < 9; i++) if (board[i] == ' ') found = true;
    if (!found) return;

    // Пошук випадкової вільної клітинки
    do {
        choice = rand() % 9;
    } while (board[choice] != ' ');

    board[choice] = 'O';
    SetWindowText(buttons[choice], L"O");

    if (CheckWinner('O')) {
        gameOver = true;
        MessageBox(NULL, L"Bot Wins!", L"End", MB_OK);
    }
}

// Обробка натискань на кнопки
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        // Створення 9 ігрових кнопок
        for (int i = 0; i < 9; i++) {
            buttons[i] = CreateWindow(L"BUTTON", L"", WS_VISIBLE | WS_CHILD,
                (i % 3) * 100, (i / 3) * 100, 100, 100, hwnd, (HMENU)i, NULL, NULL);
        }
        // Створюємо кнопку Retry
        CreateWindow(L"BUTTON", L"Retry", WS_VISIBLE | WS_CHILD, 100, 320, 100, 40, hwnd, (HMENU)10, NULL, NULL);
        return 0;

    case WM_COMMAND: {
        int id = LOWORD(wParam);

        // Якщо натиснули Retry
        if (id == 10) {
            gameOver = false;
            for (int i = 0; i < 9; i++) {
                board[i] = ' ';
                SetWindowText(buttons[i], L"");
            }
            return 0;
        }

        // Якщо натиснули на ігрову клітинку
        if (id >= 0 && id < 9 && board[id] == ' ' && !gameOver) {
            board[id] = 'X';
            SetWindowText(buttons[id], L"X");

            if (CheckWinner('X')) {
                gameOver = true;
                MessageBox(hwnd, L"You Win!", L"End", MB_OK);
            }
            else {
                BotMove(); // Одразу після гравця ходить бот
            }
        }
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}