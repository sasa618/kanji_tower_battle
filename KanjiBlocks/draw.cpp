//GDIで図形や文字を描く
#include "draw.h"

//一度に描く多角形の頂点数の上限
const int maxDrawPointCnt = 32;

//多角形を塗りつぶして枠線を引く
void FillPolygon(HDC hdc, const VEC2* points, int pointCnt, UINT fillRGB, UINT lineRGB) {
	if(pointCnt > maxDrawPointCnt) pointCnt = maxDrawPointCnt;

	//描画APIの都合で整数にする(四捨五入)
	POINT drawPoints[maxDrawPointCnt];
	for(int i=0; i<pointCnt; i++) {
		drawPoints[i].x = (int)(points[i].x + 0.5f);
		drawPoints[i].y = (int)(points[i].y + 0.5f);
	}

	HBRUSH hBrush = CreateSolidBrush(fillRGB);
	HPEN hPen = CreatePen(PS_SOLID, 1, lineRGB);

	HGDIOBJ hOldBrush = SelectObject(hdc, hBrush);
	HGDIOBJ hOldPen = SelectObject(hdc, hPen);

	Polygon(hdc, drawPoints, pointCnt);

	SelectObject(hdc, hOldBrush);
	SelectObject(hdc, hOldPen);
	DeleteObject(hBrush);
	DeleteObject(hPen);
}

//長方形を塗りつぶす
void FillBox(HDC hdc, int left, int top, int right, int bottom, UINT rgb) {
	RECT rect = {left, top, right, bottom};
	HBRUSH hBrush = CreateSolidBrush(rgb);

	FillRect(hdc, &rect, hBrush);

	DeleteObject(hBrush);
}

//文字を描く(背景は塗らない)
void DrawString(HDC hdc, int x, int y, const wchar_t* text, UINT rgb) {
	static HFONT hFont = NULL;  //最初の1回だけ作って使い回す

	if(hFont == NULL) {
		hFont = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Meiryo UI");
	}

	HGDIOBJ hOldFont = SelectObject(hdc, hFont);
	SetBkMode(hdc, TRANSPARENT);
	SetTextColor(hdc, rgb);

	TextOutW(hdc, x, y, text, lstrlenW(text));

	SelectObject(hdc, hOldFont);
}
