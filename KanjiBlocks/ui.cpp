/**
 * @file ui.cpp
 * @brief 画面の部品(ボタンとメニュー)と、画面の表示に使う描画の実装です。
 *
 * このファイルは AI(Claude)が書いたものです。
 */
#include "ui.h"

#include <algorithm>
#include <utility>

#include "config.h"
#include "draw.h"

#pragma comment(lib, "msimg32.lib")  // AlphaBlend に使います。

namespace {

/** ボタンの文字の高さ(px)です。 */
constexpr int kButtonTextSize = 24;

/** ボタンの角の丸みの半径(px)です。 */
constexpr int kButtonRadius = 12;

/** 一度に描く多角形の頂点の数の上限です。 */
constexpr int kMaxPolygonPoints = 32;

}  // namespace

void FillRoundBox(HDC hdc, const RECT& box, int radius, UINT fill_color, UINT line_color) {
    const HBRUSH brush = CreateSolidBrush(fill_color);
    const HPEN pen = CreatePen(PS_SOLID, 2, line_color);
    const HGDIOBJ old_brush = SelectObject(hdc, brush);
    const HGDIOBJ old_pen = SelectObject(hdc, pen);

    RoundRect(hdc, box.left, box.top, box.right, box.bottom, radius * 2, radius * 2);

    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void FillBoxAlpha(HDC hdc, const RECT& box, UINT color, int alpha) {
    // 1画素の画像を作り、引き伸ばして重ねます。
    const HDC memory_dc = CreateCompatibleDC(hdc);
    const HBITMAP bitmap = CreateCompatibleBitmap(hdc, 1, 1);
    const HGDIOBJ old_bitmap = SelectObject(memory_dc, bitmap);
    SetPixel(memory_dc, 0, 0, color);

    const BLENDFUNCTION blend = {AC_SRC_OVER, 0, static_cast<BYTE>(std::clamp(alpha, 0, 255)), 0};
    AlphaBlend(hdc, box.left, box.top, box.right - box.left, box.bottom - box.top, memory_dc, 0, 0, 1, 1, blend);

    SelectObject(memory_dc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(memory_dc);
}

void FillThickPolygon(HDC hdc, const VEC2* points, int point_count, UINT color, int line_width) {
    point_count = (std::min)(point_count, kMaxPolygonPoints);

    POINT draw_points[kMaxPolygonPoints];
    for (int i = 0; i < point_count; ++i) {
        draw_points[i].x = static_cast<int>(points[i].x + 0.5f);
        draw_points[i].y = static_cast<int>(points[i].y + 0.5f);
    }

    const HBRUSH brush = CreateSolidBrush(color);
    const HPEN pen = CreatePen(PS_SOLID, line_width, color);
    const HGDIOBJ old_brush = SelectObject(hdc, brush);
    const HGDIOBJ old_pen = SelectObject(hdc, pen);

    Polygon(hdc, draw_points, point_count);

    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    DeleteObject(brush);
    DeleteObject(pen);
}

Button::Button(std::wstring text, const RECT& box) : text_(std::move(text)), box_(box) {}

bool Button::Contains(int x, int y) const {
    return x >= box_.left && x < box_.right && y >= box_.top && y < box_.bottom;
}

void Button::Render(HDC hdc, bool is_selected) const {
    FillRoundBox(hdc, box_, kButtonRadius, is_selected ? kSelectColor : kBackgroundColor, kFrameColor);

    const int center_x = (box_.left + box_.right) / 2;
    const int text_top = (box_.top + box_.bottom - kButtonTextSize) / 2;
    DrawCenteredText(hdc, center_x, text_top, text_.c_str(), kButtonTextSize, kTextColor);
}

void Menu::Clear() {
    buttons_.clear();
    selected_ = 0;
}

void Menu::Add(const std::wstring& text, const RECT& box) {
    buttons_.emplace_back(text, box);
}

int Menu::OnKeyDown(int key) {
    const int count = static_cast<int>(buttons_.size());
    if (count == 0) {
        return -1;
    }

    switch (key) {
        case VK_UP:
        case VK_LEFT:
            selected_ = (selected_ + count - 1) % count;
            return -1;
        case VK_DOWN:
        case VK_RIGHT:
            selected_ = (selected_ + 1) % count;
            return -1;
        case VK_RETURN:
        case VK_SPACE:
            return selected_;
        default:
            return -1;
    }
}

void Menu::OnMouseMove(int x, int y) {
    const int index = Find(x, y);
    if (index >= 0) {
        selected_ = index;
    }
}

int Menu::OnMouseDown(int x, int y) {
    const int index = Find(x, y);
    if (index >= 0) {
        selected_ = index;
    }
    return index;
}

void Menu::Render(HDC hdc) const {
    for (int i = 0; i < static_cast<int>(buttons_.size()); ++i) {
        buttons_[i].Render(hdc, i == selected_);
    }
}

int Menu::Find(int x, int y) const {
    for (int i = 0; i < static_cast<int>(buttons_.size()); ++i) {
        if (buttons_[i].Contains(x, y)) {
            return i;
        }
    }
    return -1;
}
