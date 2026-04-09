#include <windows.h>
#include <ctime>
#include <vector>
#include <algorithm>

// Глобальні змінні
char board[9];              // Ігрове поле
HWND buttons[9];            // Кнопки ігрової сітки
HWND btnEasy, btnHard;      // Кнопки вибору складності
bool gameOver = false;      // Прапорець завершення гри
bool hardMode = false;      // Режим складності (true - важкий, false - легкий)
bool gameActive = false;    // Чи активна гра в даний момент (чи показана сітка)

// Прототип функції обробки повідомлень вікна
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Функція для скидання ігрової логіки перед новою партією
void ResetGame() {
    for (int i = 0; i < 9; i++) {
        board[i] = ' ';                // Очищаємо масив
        SetWindowText(buttons[i], L""); // Очищаємо текст на кнопках у вікні
    }
    gameOver = false;
    gameActive = true;
}

// Функція для перемикання між головним меню та ігровим полем
void ShowMenu(bool menuVisible, HWND hwnd) {
    int menuCmd = menuVisible ? SW_SHOW : SW_HIDE; // Режим відображення меню
    int gameCmd = menuVisible ? SW_HIDE : SW_SHOW; // Режим відображення сітки

    // Показуємо/ховаємо кнопки вибору складності
    ShowWindow(btnEasy, menuCmd);
    ShowWindow(btnHard, menuCmd);

    // Показуємо/ховаємо ігрові кнопки
    for (int i = 0; i < 9; i++) {
        ShowWindow(buttons[i], gameCmd);
    }
}

// Оцінка стану поля
int Evaluate() {
    // Всі можливі виграшні комбінації
    int w[8][3] = { {0,1,2}, {3,4,5}, {6,7,8}, {0,3,6}, {1,4,7}, {2,5,8}, {0,4,8}, {2,4,6} };
    for (int i = 0; i < 8; i++) {
        if (board[w[i][0]] != ' ' && board[w[i][0]] == board[w[i][1]] && board[w[i][1]] == board[w[i][2]]) {
            if (board[w[i][0]] == 'O') return +10; // Виграв бот
            if (board[w[i][0]] == 'X') return -10; // Виграв гравець
        }
    }
    return 0; // Ніхто не виграв або нічия
}

// Перевірка, чи залишилися вільні клітинки
bool MovesLeft() {
    for (int i = 0; i < 9; i++) if (board[i] == ' ') return true;
    return false;
}

// Алгоритм Minimax (прорахунок всіх варіантів ходів)
int Minimax(int depth, bool isMax) {
    int score = Evaluate();
    if (score == 10 || score == -10) return score;
    if (!MovesLeft()) return 0;

    if (isMax) { // Хід бота
        int best = -1000;
        for (int i = 0; i < 9; i++) {
            if (board[i] == ' ') {
                board[i] = 'O';
                best = (std::max)(best, Minimax(depth + 1, !isMax));
                board[i] = ' ';
            }
        }
        return best;
    }
    else { // Хід гравця
        int best = 1000;
        for (int i = 0; i < 9; i++) {
            if (board[i] == ' ') {
                board[i] = 'X';
                best = (std::min)(best, Minimax(depth + 1, !isMax));
                board[i] = ' ';
            }
        }
        return best;
    }
}

// Основна функція ходу бота
void BotMove() {
    if (gameOver || !MovesLeft()) return;
    int move = -1;

    // Складний режим (90% часу бот грає через Minimax)
    if (hardMode && (rand() % 100 >= 10)) {
        int bestVal = -1000;
        for (int i = 0; i < 9; i++) {
            if (board[i] == ' ') {
                board[i] = 'O';
                int moveVal = Minimax(0, false);
                board[i] = ' ';
                if (moveVal > bestVal) { move = i; bestVal = moveVal; }
            }
        }
    }
    else {
        // Легкий режим (10% шансу випадкового ходу)
        do { move = rand() % 9; } while (board[move] != ' ');
    }

    // Виконуємо хід у вікні
    if (move != -1) {
        board[move] = 'O';
        SetWindowText(buttons[move], L"O");
        if (Evaluate() == 10) {
            gameOver = true;
            MessageBox(NULL, L"Bot Wins!", L"Game Over", MB_OK);
            ShowMenu(true, NULL); // Повернення в меню після поразки
        }
    }
}

// Точка входу в програму (Main)
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR pCmd, int nShow) {
    srand(time(0)); // Ініціалізація генератора випадкових чисел
    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"TicTacToeClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClass(&wc);

    // Створення головного вікна
    HWND hwnd = CreateWindowEx(0, L"TicTacToeClass", L"Tic Tac Toe - AI Edition", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 316, 340, NULL, NULL, hInst, NULL);

    ShowWindow(hwnd, nShow);
    MSG msg = { };
    // Цикл обробки повідомлень
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

// Функція обробки подій (натискання кнопок, закриття вікна тощо)
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        // Створення 9 ігрових кнопок (спочатку приховані)
        for (int i = 0; i < 9; i++) {
            board[i] = ' ';
            buttons[i] = CreateWindow(L"BUTTON", L"", WS_CHILD,
                (i % 3) * 100, (i / 3) * 100, 100, 100, hwnd, (HMENU)i, NULL, NULL);
        }
        // Створення кнопок меню (видимі при запуску)
        btnEasy = CreateWindow(L"BUTTON", L"PLAY EASY", WS_VISIBLE | WS_CHILD, 50, 80, 200, 50, hwnd, (HMENU)11, NULL, NULL);
        btnHard = CreateWindow(L"BUTTON", L"PLAY HARD", WS_VISIBLE | WS_CHILD, 50, 150, 200, 50, hwnd, (HMENU)12, NULL, NULL);
        return 0;

    case WM_COMMAND: {
        int id = LOWORD(wParam);

        // Обробка вибору складності (ID 11 та 12)
        if (id == 11 || id == 12) {
            hardMode = (id == 12);
            ResetGame();
            ShowMenu(false, hwnd); // Ховаємо меню, показуємо гру
            return 0;
        }

        // Обробка натискання на ігрову клітинку (ID 0-8)
        if (id >= 0 && id < 9 && board[id] == ' ' && !gameOver && gameActive) {
            board[id] = 'X'; // Хід гравця
            SetWindowText(buttons[id], L"X");

            // Перевірка результату після ходу гравця
            if (Evaluate() == -10) {
                gameOver = true;
                MessageBox(hwnd, L"You Win!", L"Game Over", MB_OK);
                ShowMenu(true, hwnd); // Повернення в меню
            }
            else if (!MovesLeft()) {
                gameOver = true;
                MessageBox(hwnd, L"Draw!", L"Game Over", MB_OK);
                ShowMenu(true, hwnd); // Повернення в меню
            }
            else {
                BotMove(); // Передача ходу боту
                // Перевірка на нічию після ходу бота
                if (!MovesLeft() && !gameOver) {
                    gameOver = true;
                    MessageBox(hwnd, L"Draw!", L"Game Over", MB_OK);
                    ShowMenu(true, hwnd);
                }
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
