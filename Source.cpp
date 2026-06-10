#include <windows.h>
#include <ctime>
#include <vector>
#include <algorithm>
#include <string>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "Msimg32.lib")

// Ігрове поле: масив із 9 символів (' ', 'X', 'O')
char board[9];

// Ідентифікатори вікон для 9 кнопок ігрової сітки
HWND buttons[9];

// Ідентифікатори керуючих елементів головного меню та інтерфейсу
HWND btnEasy, btnMedium, btnHard, btnTheme, btnBackToMenu;

// Прапорці стану поточної ігрової сесії
bool gameOver = false;
bool gameActive = false;
bool isGreenTheme = false;
bool isBotThinking = false;
int difficultyMode = 0;

// Затримка виведення результату гри
int pendingResult = 0;

// Фінальний графічний результат гри
std::wstring endResultText = L"";

// Визначення кольорів у форматі RGB для синьої та зеленої тем оформлення
COLORREF blueBg = RGB(20, 30, 50), blueCard = RGB(45, 60, 90), blueAccent = RGB(0, 225, 255), blueAlt = RGB(190, 130, 255);
COLORREF greenBg = RGB(20, 45, 25), greenCard = RGB(55, 85, 60), greenAccent = RGB(120, 255, 150), greenAlt = RGB(230, 255, 120);
COLORREF curBg, curCard, curAccent, curAlt, curText = RGB(245, 245, 255);

/**
 * Відтворення звукових ефектів та фонової музики через Media Control Interface (MCI)
 * @param filename Назва звукового файлу без розширення
 * @param isMusic Прапорець фонової аудіодоріжки (циклічне відтворення)
 */
void PlayGameSound(LPCWSTR filename, bool isMusic = false) {
    std::wstring file = std::wstring(filename) + L".mp3";
    if (isMusic) {
        // Керування фоновою музикою за допомогою аліасу "bgm"
        mciSendStringW(L"close bgm", NULL, 0, NULL);
        std::wstring cmdOpen = L"open \"" + file + L"\" type mpegvideo alias bgm";
        if (mciSendStringW(cmdOpen.c_str(), NULL, 0, NULL) == 0) {
            mciSendStringW(L"play bgm repeat", NULL, 0, NULL); // Повторення треку по колу
        }
    }
    else {
        // Керування звуковими ефектами (сfx) подій кліку чи фіналу
        mciSendStringW(L"close sfx", NULL, 0, NULL);
        std::wstring cmdOpen = L"open \"" + file + L"\" type mpegvideo alias sfx";
        if (mciSendStringW(cmdOpen.c_str(), NULL, 0, NULL) == 0) {
            mciSendStringW(L"play sfx from 0", NULL, 0, NULL); // Відтворення з початку файлу
        }
    }
}

/**
 * Динамічне перемикання активної палітри кольорів відповідно до обраної теми
 */
void UpdateThemeColors() {
    if (isGreenTheme) { curBg = greenBg; curCard = greenCard; curAccent = greenAccent; curAlt = greenAlt; }
    else { curBg = blueBg; curCard = blueCard; curAccent = blueAccent; curAlt = blueAlt; }
}

/**
 * Малювання вертикального градієнта за допомогою Win32 GDI
 * @param ontextGradient Контекст градфенту для малювання
 * @param rectForGradient Межі області промальовування
 * @param topGradientColor Колір верхньої межі градієнта
 * @param bottomGradientColor Колір нижньої межі градієнта
 */
void DrawGradient(HDC ontextGradient, RECT rectForGradient, COLORREF topGradientColor, COLORREF bottomGradientColor) {
    TRIVERTEX v[2];
    // Конвертація 8-бітних кольорів RGB у 16-бітні значення для функції GradientFill
    v[0].x = rectForGradient.left; v[0].y = rectForGradient.top; v[0].Red = (COLOR16)(GetRValue(topGradientColor) << 8); v[0].Green = (COLOR16)(GetGValue(topGradientColor) << 8); v[0].Blue = (COLOR16)(GetBValue(topGradientColor) << 8); v[0].Alpha = 0;
    v[1].x = rectForGradient.right; v[1].y = rectForGradient.bottom; v[1].Red = (COLOR16)(GetRValue(bottomGradientColor) << 8); v[1].Green = (COLOR16)(GetGValue(bottomGradientColor) << 8); v[1].Blue = (COLOR16)(GetBValue(bottomGradientColor) << 8); v[1].Alpha = 0;
    GRADIENT_RECT g = { 0, 1 };
    GradientFill(ontextGradient, v, 2, &g, 1, GRADIENT_FILL_RECT_V); // Системний рендеринг вертикального градієнта
}

/**
 * Перемикання станів відображення між вікном головного меню та ігровим полем
 * @param isVisibleMainMenu true — показати головне меню, false — приховати його та показати ігрову сітку
 * @param hwnd Дескриптор головного вікна програми
 */
void ShowMenu(bool isVisibleMainMenu, HWND hwnd) {
    int cmd = isVisibleMainMenu ? SW_SHOW : SW_HIDE;
    // Зміна стану видимості кнопок режимів та налаштувань
    ShowWindow(btnEasy, cmd); ShowWindow(btnMedium, cmd); ShowWindow(btnHard, cmd); ShowWindow(btnTheme, cmd);

    // Відображення або приховування ігрових кнопок сітки 3х3
    for (int i = 0; i < 9; i++) ShowWindow(buttons[i], isVisibleMainMenu ? SW_HIDE : SW_SHOW);
    ShowWindow(btnBackToMenu, SW_HIDE);

    gameActive = !isVisibleMainMenu;
    if (isVisibleMainMenu) {
        gameOver = false;
        isBotThinking = false;
        endResultText = L"";
    }
    // Надсилання запиту операційній системі на повне перемалювання вікна
    InvalidateRect(hwnd, NULL, TRUE);
}

/**
 * Статичний аналізатор поточної матриці ігрового поля на предмет переможних комбінацій
 * @return 10 — переміг бот ('O'), -10 — переміг гравець ('X'), 0 — комбінацій не виявлено
 */
int Evaluate() {
    // Двовимірний масив усіх 8 можливих виграшних ліній (горизонталі, вертикалі, діагоналі)
    int w[8][3] = { {0,1,2},{3,4,5},{6,7,8},{0,3,6},{1,4,7},{2,5,8},{0,4,8},{2,4,6} };
    for (auto& r : w) {
        if (board[r[0]] != ' ' && board[r[0]] == board[r[1]] && board[r[1]] == board[r[2]]) {
            return (board[r[0]] == 'O') ? 10 : -10;
        }
    }
    return 0;
}

/**
 * Рекурсивний алгоритм Minimax для пошуку оптимальних ходів у грі з нульовою сумою
 * @param depth Поточна глибина дерева рекурсивних викликів
 * @param isMax true — хід максимізуючого гравця (ШІ), false — хід мінімізуючого (людина)
 */
int Minimax(int depth, bool isMax) {
    int score = Evaluate();
    // Оцінка термінальних станів із врахуванням глибини для вибору найкоротшого шляху до перемоги
    if (score == 10) return score - depth;
    if (score == -10) return score + depth;

    bool left = false;
    for (int i = 0; i < 9; i++) if (board[i] == ' ') left = true;
    if (!left) return 0; // Стан нічиєї (вільних клітинок немає)

    if (isMax) {
        int best = -1000;
        for (int i = 0; i < 9; i++) {
            if (board[i] == ' ') {
                board[i] = 'O'; // Симуляція ходу бота
                best = (std::max)(best, Minimax(depth + 1, false));
                board[i] = ' '; // Відкат стану поля (Backtracking)
            }
        }
        return best;
    }
    else {
        int best = 1000;
        for (int i = 0; i < 9; i++) {
            if (board[i] == ' ') {
                board[i] = 'X'; // Симуляція ходу користувача
                best = (std::min)(best, Minimax(depth + 1, true));
                board[i] = ' '; // Відкат стану поля (Backtracking)
            }
        }
        return best;
    }
}

/**
 * Процедура фіксації завершення гри: приховує поле та ініціює виведення результатів
 * @param hwnd Дескриптор головного вікна
 * @param res Код результату гри з функції Evaluate()
 */
void HandleGameEnd(HWND hwnd, int res) {
    gameOver = true;
    isBotThinking = false;
    PlayGameSound(L"end");

    if (res == 10) endResultText = L"AI WINS!";
    else if (res == -10) endResultText = L"VICTORY!";
    else endResultText = L"DRAW!";

    for (int i = 0; i < 9; i++) ShowWindow(buttons[i], SW_HIDE);

    ShowWindow(btnBackToMenu, SW_SHOW);
    InvalidateRect(hwnd, NULL, TRUE); // Запит повідомлення WM_PAINT для малювання тексту фіналу
}

/**
 * Логічний модуль розрахунку та виконання ходу автономного цифрового опонента (бота)
 */
void ExecuteBotMove(HWND hwnd) {
    isBotThinking = false;
    if (gameOver) return;

    int move = -1, chance = rand() % 100;
    // Визначення порогу ймовірності використання ідеального алгоритму залежно від режиму
    int threshold = (difficultyMode == 0) ? -1 : (difficultyMode == 1 ? 50 : 80);

    if (chance < threshold) {
        int bestVal = -1000;
        for (int i = 0; i < 9; i++) {
            if (board[i] == ' ') {
                board[i] = 'O';
                int v = Minimax(0, false);
                board[i] = ' ';
                if (v > bestVal) { bestVal = v; move = i; }
            }
        }
    }
    // Елемент випадковості: якщо алгоритм Minimax не задіявся, або для легкого режиму
    if (move == -1) {
        std::vector<int> a; for (int i = 0; i < 9; i++) if (board[i] == ' ') a.push_back(i);
        if (!a.empty()) move = a[rand() % a.size()];
    }

    if (move != -1) {
        board[move] = 'O';
        PlayGameSound(L"click");

        // Примусовий рендеринг: UpdateWindow змушує систему Windows в той же 
        // мікросегмент секунди оновити кнопку, усуваючи затримку черги повідомлень
        InvalidateRect(buttons[move], NULL, TRUE);
        UpdateWindow(buttons[move]);

        int res = Evaluate();
        if (res == 10 || std::count(board, board + 9, ' ') == 0) {
            pendingResult = res;
            isBotThinking = true; // Тимчасове блокування дій гравця під час фінальної паузи
            SetTimer(hwnd, 2, 350, NULL); // Запуск системного таймера фінальної затримки (ID = 2)
        }
    }
}

/**
 * Головна зворотна віконна процедура (Window Procedure) — ядро обробки повідомлень Win32 ОС
 */
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: // Повідомлення при ініціалізації та створенні вікна програми
        // Динамічне створення 9 дочірніх кнопок ігрового поля зі стилем BS_OWNERDRAW
        for (int i = 0; i < 9; i++) buttons[i] = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | BS_OWNERDRAW, (i % 3) * 100, (i / 3) * 100, 100, 100, hwnd, (HMENU)(INT_PTR)i, NULL, NULL);
        // Створення кнопок головного меню вибору режимів та зміни колірних тем
        btnEasy = CreateWindowExW(0, L"BUTTON", L"EASY", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 50, 40, 200, 50, hwnd, (HMENU)11, NULL, NULL);
        btnMedium = CreateWindowExW(0, L"BUTTON", L"MEDIUM", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 50, 100, 200, 50, hwnd, (HMENU)13, NULL, NULL);
        btnHard = CreateWindowExW(0, L"BUTTON", L"HARD", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 50, 160, 200, 50, hwnd, (HMENU)12, NULL, NULL);
        btnTheme = CreateWindowExW(0, L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 130, 310, 40, 40, hwnd, (HMENU)14, NULL, NULL);
        btnBackToMenu = CreateWindowExW(0, L"BUTTON", L"BACK TO MENU", WS_CHILD | BS_OWNERDRAW, 50, 200, 200, 50, hwnd, (HMENU)15, NULL, NULL);
        return 0;

    case WM_TIMER: // Обробка повідомлень від системних таймерів SetTimer
        if (wParam == 1) {
            KillTimer(hwnd, 1); // Зупинка таймера імітації "мислення" бота (ID = 1)
            ExecuteBotMove(hwnd);
        }
        else if (wParam == 2) {
            KillTimer(hwnd, 2); // Зупинка таймера кінематографічної фінальної паузи (ID = 2)
            HandleGameEnd(hwnd, pendingResult); // Виклик процедури переходу до екрана підсумків
        }
        return 0;

    case WM_PAINT: { // Стандартний обробник перемальовування графічного інтерфейсу головного вікна
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps); // Початок графічного рендерингу на контексті вікна
        RECT r; GetClientRect(hwnd, &r);
        DrawGradient(hdc, r, curBg, curCard); // Промальовування фонового градієнта вікна

        if (gameOver && !endResultText.empty()) {
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, curAccent);
            // Створення кастомного шрифту Arial великого розміру для відображення результату гри
            HFONT hFont = CreateFontW(36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");
            HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

            RECT textRect = { 0, 100, 300, 160 };
            DrawTextW(hdc, endResultText.c_str(), -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            SelectObject(hdc, hOldFont); // Повернення базового системного шрифту
            DeleteObject(hFont); // Звільнення пам'яті GDI від створеного шрифту
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DRAWITEM: { // Кастомізація дизайну елементів керування за рахунок обробки події BS_OWNERDRAW
        LPDRAWITEMSTRUCT d = (LPDRAWITEMSTRUCT)lParam;
        bool p = (d->itemState & ODS_SELECTED); // Визначення, чи натиснута кнопка в даний момент

        if (d->CtlID == 14) { // Рендеринг кнопки зміни теми оформлення
            HBRUSH b = CreateSolidBrush(isGreenTheme ? blueAccent : greenAccent);
            HPEN pen = CreatePen(PS_SOLID, 2, curText); SelectObject(d->hDC, b); SelectObject(d->hDC, pen);
            Ellipse(d->hDC, 2, 2, 38, 38); DeleteObject(b); DeleteObject(pen); return TRUE;
        }

        // Рендеринг закругленого дизайну (картки) для звичайних кнопок меню та поля
        DrawGradient(d->hDC, d->rcItem, p ? curBg : curCard, curBg);
        HPEN hF = CreatePen(PS_SOLID, 1, RGB(120, 120, 140)); SelectObject(d->hDC, hF); SelectObject(d->hDC, GetStockObject(NULL_BRUSH));
        RoundRect(d->hDC, d->rcItem.left + 1, d->rcItem.top + 1, d->rcItem.right - 1, d->rcItem.bottom - 1, 12, 12);

        if (d->CtlID < 9) { // Низькорівневе малювання фігур хрестиків та нуликів на кнопках поля
            int off = p ? 2 : 0;
            // Створення товстого графічного пера (HPEN) для чіткого відмальовування геометрії фігур
            HPEN hS = CreatePen(PS_SOLID, 7, (board[d->CtlID] == 'X') ? curAccent : curAlt);
            SelectObject(d->hDC, hS);

            if (board[d->CtlID] == 'X') {
                // Малювання ліній хрестика за заданими координатами вектора зміщення
                MoveToEx(d->hDC, 30 + off, 30 + off, NULL); LineTo(d->hDC, 70 + off, 70 + off);
                MoveToEx(d->hDC, 70 + off, 30 + off, NULL); LineTo(d->hDC, 30 + off, 70 + off);
            }
            else if (board[d->CtlID] == 'O') {
                // Малювання кола нулика
                Ellipse(d->hDC, 28 + off, 28 + off, 72 + off, 72 + off);
            }
            DeleteObject(hS); // Видалення пера для уникнення витоку пам'яті в GDI підсистемі
        }
        else { // Рендеринг текстових написів на функціональних кнопках меню
            SetBkMode(d->hDC, TRANSPARENT); SetTextColor(d->hDC, curText);
            wchar_t t[32]; GetWindowTextW(d->hwndItem, t, 32);
            DrawTextW(d->hDC, t, -1, &d->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        DeleteObject(hF); return TRUE;
    }
    case WM_COMMAND: { // Обробка подій взаємодії користувача з елементами (кліки на кнопки)
        int id = LOWORD(wParam);

        if (id == 14) { // Клік на кнопку зміни візуальної теми програми
            isGreenTheme = !isGreenTheme; UpdateThemeColors();
            // Заміна глобального кольору заповнення фону класу вікна
            SetClassLongPtrW(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)CreateSolidBrush(curBg));
            InvalidateRect(hwnd, NULL, TRUE); PlayGameSound(L"click"); return 0;
        }

        if (id == 15) { // Клік на кнопку повернення до головного меню
            PlayGameSound(L"click");
            ShowMenu(true, hwnd);
            return 0;
        }

        if (id >= 11 && id <= 13) { // Кліки на кнопки встановлення складності та старту гри
            difficultyMode = (id == 11 ? 0 : (id == 13 ? 1 : 2));
            for (int i = 0; i < 9; i++) board[i] = ' '; // Очищення матриці поля
            ShowMenu(false, hwnd);
            PlayGameSound(L"click");
            return 0;
        }

        // Обробка ходу користувача
        if (id < 9 && board[id] == ' ' && !gameOver && gameActive && !isBotThinking) {
            board[id] = 'X'; // Фіксація ходу гравця в масив програми
            PlayGameSound(L"click");

            // Примусовий рендеринг: функція UpdateWindow надсилає повідомлення
            // WM_PAINT безпосередньо кнопці в той самий момент часу, усуваючи затримку
            InvalidateRect(buttons[id], NULL, TRUE);
            UpdateWindow(buttons[id]);

            int res = Evaluate();
            if (res == -10 || std::count(board, board + 9, ' ') == 0) {
                pendingResult = res;
                isBotThinking = true;
                SetTimer(hwnd, 2, 350, NULL); // Акцивація таймера затримки фіналу (350 мс)
            }
            else {
                isBotThinking = true;
                SetTimer(hwnd, 1, 600, NULL); // Активація таймера імітації "думок" бота (600 мс)
            }
        }
        return 0;
    }
    case WM_DESTROY: // Повідомлення при закритті вікна користувачем
        mciSendStringW(L"close bgm", NULL, 0, NULL); // Повне звільнення аудіо-рушія фонової музики
        mciSendStringW(L"close sfx", NULL, 0, NULL); // Звільнення аудіо-рушія ефектів
        PostQuitMessage(0); // Ініціація виходу з циклу обробки повідомлень програми
        return 0;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam); // Передача нереалізованих повідомлень системі
}

/**
 * Головна точка входу для Windows-додатків
 */
int WINAPI WinMain(_In_ HINSTANCE hI, _In_opt_ HINSTANCE hP, _In_ LPSTR lpC, _In_ int nS) {
    srand((unsigned int)time(0)); // Ініціалізація генератора випадкових чисел
    UpdateThemeColors();

    // Реєстрація структури класу вікна програми в операційній системі Windows
    WNDCLASSW wc = { 0 }; wc.lpfnWndProc = WindowProc; wc.hInstance = hI; wc.lpszClassName = L"TicTacToe";
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW); wc.hbrBackground = CreateSolidBrush(curBg);
    RegisterClassW(&wc);

    // Безпосереднє створення вікна із фіксованими системними розмірами інтерфейсу
    HWND hwnd = CreateWindowExW(WS_EX_APPWINDOW, L"TicTacToe", L"Tic Tac Toe", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, 316, 430, NULL, NULL, hI, NULL);

    PlayGameSound(L"music", true); // Старт відтворення фонової аудіодоріжки гри

    ShowWindow(hwnd, nS);

    // Цикл обробки та диспетчеризації системних повідомлень (Event Loop / Message Pump)
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg); // Трансляція повідомлень віртуальних клавіш
        DispatchMessageW(&msg); // Відправка повідомлення у віконну процедуру WindowProc
    }
    return 0;
}
