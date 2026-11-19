/**
 * @file kanji_draw.cpp
 * @brief 物体と字の形の描画の実装です。
 *
 * このファイルは AI(Claude)が書いたものです。
 */
#include "kanji_draw.h"

#include "draw.h"

void DrawBody(HDC hdc, BODY* body, UINT color, float offset_y) {
    VEC2 points[maxPointCnt];
    for (int part = 0; part < body->GetPartCnt(); ++part) {
        body->GetWorldPoints(part, points);
        for (int i = 0; i < body->GetPointCnt(part); ++i) {
            points[i].y -= offset_y;
        }
        FillPolygon(hdc, points, body->GetPointCnt(part), color, color);
    }
}

void DrawKanji(HDC hdc, Kanji kind, VEC2 center, float scale) {
    DrawKanji(hdc, kind, center, scale, GetKanjiColor(kind));
}

void DrawKanji(HDC hdc, Kanji kind, VEC2 center, float scale, UINT color) {
    BODY sample;
    sample.Initialize(VEC2(), 0.0f, color);
    BuildKanji(&sample, kind);

    VEC2 points[maxPointCnt];
    for (int part = 0; part < sample.GetPartCnt(); ++part) {
        sample.GetWorldPoints(part, points);
        for (int i = 0; i < sample.GetPointCnt(part); ++i) {
            points[i] = center + points[i] * scale;
        }
        FillPolygon(hdc, points, sample.GetPointCnt(part), color, color);
    }
}
