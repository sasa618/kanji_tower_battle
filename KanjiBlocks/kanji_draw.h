/**
 * @file kanji_draw.h
 * @brief 物体と字の形を描きます。
 *
 * このファイルは AI(Claude)が書いたものです。
 */
#pragma once

#include "kanji.h"

/**
 * 物体を描きます。部品ごとに多角形を描き、枠線も同じ色にして画の継ぎ目を見せません。
 * @param color 色
 * @param offset_y 画面をずらした分です。描く位置をこの分だけ下げます。
 */
void DrawBody(HDC hdc, BODY* body, UINT color, float offset_y);

/**
 * 字の形を、重心が center に来るように scale 倍で描きます。
 * @param center 画面の位置
 * @param scale 大きさの倍率
 */
void DrawKanji(HDC hdc, Kanji kind, VEC2 center, float scale);

/**
 * 字の形を、字の色ではなく color の1色で描きます。まだ見つけていない字の形を薄く見せるのに使います。
 * @param center 画面の位置
 * @param scale 大きさの倍率
 */
void DrawKanji(HDC hdc, Kanji kind, VEC2 center, float scale, UINT color);
