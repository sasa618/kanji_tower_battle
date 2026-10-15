//GDIで図形や文字を描く
#pragma once
#include <windows.h>
#include "vec2.h"

void FillPolygon(HDC hdc, const VEC2* points, int pointCnt, UINT fillRGB, UINT lineRGB);
void FillBox(HDC hdc, int left, int top, int right, int bottom, UINT rgb);
void DrawString(HDC hdc, int x, int y, const wchar_t* text, UINT rgb);
