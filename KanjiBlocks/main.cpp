//Win32の窓を作り、実際の時間に合わせてゲームを進めて描く
#include <windows.h>
#include <stdlib.h>  //srand用
#include <time.h>  //乱数の種に今の時刻を使うため
#include "game.h"

#pragma comment(lib, "winmm.lib")  //timeBeginPeriod用

//進めていない時間をためておく上限(秒)
//処理が追いつかないときに、ためすぎないようにする
const double maxStockTime = 0.1;

//ボタンのID
const int btnRunID = 101;
const int btnResetID = 102;

//開始/停止ボタン(文字を書き換えるので持っておく)
static HWND hBtnRun = NULL;
//リセットボタン(エディタと漢字図鑑の画面では隠すので持っておく)
static HWND hBtnReset = NULL;

//ちらつき防止用の裏画面
static HDC hBackDC = NULL;
static HBITMAP hBackBitmap = NULL;

//裏画面を作る
static void CreateBackBuf(HWND hWnd) {
	HDC hdc = GetDC(hWnd);

	hBackDC = CreateCompatibleDC(hdc);
	hBackBitmap = CreateCompatibleBitmap(hdc, screenWidth, screenHeight);
	SelectObject(hBackDC, hBackBitmap);

	ReleaseDC(hWnd, hdc);
}

//裏画面を消す
static void DeleteBackBuf() {
	if(hBackDC) DeleteDC(hBackDC);
	if(hBackBitmap) DeleteObject(hBackBitmap);

	hBackDC = NULL;
	hBackBitmap = NULL;
}

//ボタンを1つ作る
static HWND CreateBtn(HWND hWnd, const wchar_t* text, int y, int id) {
	HWND hBtn = CreateWindowW(L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, y, 160, 28, hWnd, (HMENU)(INT_PTR)id, NULL, NULL);
	SendMessageW(hBtn, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

	return hBtn;
}

//開始/停止ボタンの文字を今の状態に合わせる
//ゲームの画面でないときは、ボタンを隠す(エディタと漢字図鑑の画面に重なるため)
static void UpdateRunBtn() {
	SetWindowTextW(hBtnRun, IsRunning() ? L"停止" : L"開始");

	int show = IsGameScreen() ? SW_SHOW : SW_HIDE;
	ShowWindow(hBtnRun, show);
	ShowWindow(hBtnReset, show);
}

//メインウィンドウのメッセージを処理する
static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch(message) {
		case WM_CREATE:
			hBtnRun = CreateBtn(hWnd, L"開始", 20, btnRunID);
			hBtnReset = CreateBtn(hWnd, L"リセット", 56, btnResetID);

			CreateBackBuf(hWnd);
			ResetGame();
			break;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
				case btnRunID:
					ToggleRun();
					break;
				case btnResetID:
					ResetGame();
					break;
				default:
					break;
			}

			UpdateRunBtn();
			SetFocus(hWnd);  //ボタンにフォーカスが移るとキーが効かなくなるので戻す
			break;

		case WM_KEYDOWN:
			OnKeyDown((int)wParam);
			UpdateRunBtn();
			break;

		case WM_LBUTTONDOWN:
			SetCapture(hWnd);  //窓の外までドラッグしても、離したことが分かるようにする
			OnMouseDown((short)LOWORD(lParam), (short)HIWORD(lParam));
			break;

		case WM_MOUSEMOVE:
			OnMouseMove((short)LOWORD(lParam), (short)HIWORD(lParam));
			break;

		case WM_LBUTTONUP:
			ReleaseCapture();
			OnMouseUp((short)LOWORD(lParam), (short)HIWORD(lParam));
			break;

		case WM_PAINT: {
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);

			RenderGame(hBackDC);
			BitBlt(hdc, 0, 0, screenWidth, screenHeight, hBackDC, 0, 0, SRCCOPY);

			EndPaint(hWnd, &ps);
			break;
		}

		case WM_ERASEBKGND:
			return 1;  //背景は裏画面ごと描くので消さない(ちらつき防止)

		case WM_DESTROY:
			DeleteBackBuf();
			PostQuitMessage(0);
			break;

		default:
			return DefWindowProcW(hWnd, message, wParam, lParam);
	}

	return 0;
}

//エントリーポイント
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
	//乱数の種
	//起動するときに数を渡すと、それを種にする(同じ順に字を出して試すため)
	unsigned seed = (unsigned)time(NULL);
	if(lpCmdLine[0] != L'\0') seed = (unsigned)_wtoi(lpCmdLine);

	srand(seed);

	//窓の種類を登録
	WNDCLASSW wc = {0};
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpszClassName = L"KanjiBlocks";
	RegisterClassW(&wc);

	//中身がちょうどscreenWidth×screenHeightになる大きさで窓を作る(大きさは変えられない)
	DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
	RECT rect = {0, 0, screenWidth, screenHeight};
	AdjustWindowRect(&rect, style, FALSE);

	HWND hWnd = CreateWindowW(L"KanjiBlocks", L"漢字タワーバトル", style, CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, hInstance, NULL);
	if(hWnd == NULL) return 0;

	ShowWindow(hWnd, nCmdShow);

	//時間を測る準備
	timeBeginPeriod(1);  //Sleepで待つ時間の細かさを1ミリ秒にする

	LARGE_INTEGER frequency;
	LARGE_INTEGER prev;
	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&prev);

	double stockTime = 0.0;  //まだゲームを進めていない時間(秒)

	//メッセージループ
	//メッセージがない間に、実際に過ぎた時間の分だけゲームを進める
	MSG msg;
	while(true) {
		if(PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
			if(msg.message == WM_QUIT) break;

			TranslateMessage(&msg);
			DispatchMessageW(&msg);
			continue;
		}

		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);
		stockTime += (double)(now.QuadPart - prev.QuadPart) / frequency.QuadPart;
		prev = now;

		if(stockTime > maxStockTime) stockTime = maxStockTime;

		//1フレーム分の時間がたまるまで待つ
		if(stockTime < frameTime) {
			Sleep(1);
			continue;
		}

		stockTime -= frameTime;
		UpdateGame();
		InvalidateRect(hWnd, NULL, FALSE);  //再描画命令
	}

	timeEndPeriod(1);

	return (int)msg.wParam;
}
