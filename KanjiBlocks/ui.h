/**
 * @file ui.h
 * @brief 画面の部品(ボタンとメニュー)と、画面の表示に使う描画です。
 *
 * このファイルは AI(Claude)が書いたものです。
 */
#pragma once

#include <windows.h>

#include <string>
#include <vector>

#include "vec2.h"

/** ボタンや枠の線の色です。 */
constexpr UINT kFrameColor = RGB(150, 150, 160);

/**
 * 角の丸い長方形を塗りつぶして、枠線を引きます。
 * @param radius 角の丸みの半径(px)
 */
void FillRoundBox(HDC hdc, const RECT& box, int radius, UINT fill_color, UINT line_color);

/**
 * 長方形を、下が透けて見えるように塗ります。
 * @param alpha 塗る色の濃さ。0 で透明、255 で下が見えなくなります。
 */
void FillBoxAlpha(HDC hdc, const RECT& box, UINT color, int alpha);

/**
 * 多角形を、太い縁取りを付けて1色で塗りつぶします。縁取りの角は丸くなります。
 * @param line_width 縁取りの太さ(px)
 */
void FillThickPolygon(HDC hdc, const VEC2* points, int point_count, UINT color, int line_width);

/** ボタン1つです。 */
class Button {
public:
    Button() = default;
    Button(std::wstring text, const RECT& box);

    /** 点 (x, y) がボタンの中にあれば true です。 */
    bool Contains(int x, int y) const;

    /**
     * ボタンを描きます。
     * @param is_selected 選ばれていれば true です。背景の色を変えます。
     */
    void Render(HDC hdc, bool is_selected) const;

private:
    std::wstring text_;
    RECT box_ = {};
};

/**
 * ボタンを並べたメニューです。
 * 矢印キーかマウスで選び、Enter かクリックで決めます。
 */
class Menu {
public:
    /** ボタンをすべて消します。 */
    void Clear();

    /** ボタンを後ろに足します。 */
    void Add(const std::wstring& text, const RECT& box);

    /**
     * キーが押されたときに呼びます。
     * @return ボタンを決めたときは、その番号です。決めていなければ -1 です。
     */
    int OnKeyDown(int key);

    /** マウスが動いたときに呼びます。カーソルの下のボタンを選びます。 */
    void OnMouseMove(int x, int y);

    /**
     * クリックされたときに呼びます。
     * @return ボタンを押したときは、その番号です。押していなければ -1 です。
     */
    int OnMouseDown(int x, int y);

    void Render(HDC hdc) const;

private:
    /** 点 (x, y) にあるボタンの番号を返します。無ければ -1 です。 */
    int Find(int x, int y) const;

    std::vector<Button> buttons_;
    int selected_ = 0;  ///< 選んでいるボタンの番号
};
