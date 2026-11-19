/**
 * @file view_play.cpp
 * @brief プレイ画面とリザルト画面の表示の実装です。
 *
 * このファイルは AI(Claude)が書いたものです。
 */
#include "view.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "asset.h"
#include "config.h"
#include "draw.h"
#include "game.h"
#include "kanji_draw.h"
#include "ui.h"

namespace {

// ---------------------------------------------------------------------------
// プレイ画面
// ---------------------------------------------------------------------------

// 右の欄(スコアと、落とす字の候補)の並べ方です。
constexpr int kPanelLeft = 780;
constexpr int kLineHeight = 24;
constexpr int kHandHeight = 94;  ///< 落とす字の候補1つの高さ

// 左の欄(漢字図鑑のボタンと、操作の説明の画像)の並べ方です。
constexpr RECT kCollectionButtonBox = {16, 18, 166, 66};
constexpr const wchar_t* kControlsImage = L"controls.bmp";
constexpr int kControlsLeft = 10;
constexpr int kControlsTop = 84;

// 合体できる字の光らせ方です。字の形を太い縁取りで2回描き、外側を薄く、内側を濃くします。
constexpr UINT kGlowOuterColor = RGB(255, 242, 150);
constexpr UINT kGlowInnerColor = RGB(255, 208, 0);
constexpr int kGlowInnerWidth = 7;
constexpr int kGlowOuterWidth = 13;

/** 外側の光の太さをゆらす幅(px)と、ゆれの速さ(1フレームで進む角度、ラジアン)です。 */
constexpr int kGlowWaveWidth = 8;
constexpr float kGlowWaveSpeed = 0.1f;

// 一時停止の帯です。
constexpr RECT kPauseBandBox = {0, 290, kScreenWidth, 400};
constexpr UINT kPauseBandColor = RGB(40, 40, 50);
constexpr UINT kPauseTextColor = RGB(255, 255, 255);

Image controls_image;
const Button collection_button(L"漢字図鑑", kCollectionButtonBox);
bool is_collection_button_hovered = false;

/** 静止している字の色です。少し暗くします。 */
UINT DimColor(UINT color) {
    return RGB(GetRValue(color) * 8 / 10, GetGValue(color) * 8 / 10, GetBValue(color) * 8 / 10);
}

/** 物体の形を、太い縁取りを付けて1色で描きます。 */
void DrawBodyOutline(HDC hdc, BODY* body, UINT color, int line_width, float offset_y) {
    VEC2 points[maxPointCnt];
    for (int part = 0; part < body->GetPartCnt(); ++part) {
        body->GetWorldPoints(part, points);
        for (int i = 0; i < body->GetPointCnt(part); ++i) {
            points[i].y -= offset_y;
        }
        FillThickPolygon(hdc, points, body->GetPointCnt(part), color, line_width);
    }
}

/**
 * 落とす前の字と合体できる字のまわりを、黄色く光らせます。
 * 字より先に描いて、字の後ろから光がはみ出して見えるようにします。
 */
void DrawGlows(HDC hdc, const S_GAME& game) {
    std::vector<int> targets;
    for (int i = firstKanjiIdx; i < bodyCnt; ++i) {
        if (CanMergeWithCurKanji(i)) {
            targets.push_back(i);
        }
    }

    const float wave = (std::sin(game.frames * kGlowWaveSpeed) + 1.0f) / 2.0f;
    const int outer_width = kGlowOuterWidth + static_cast<int>(kGlowWaveWidth * wave);

    for (const int index : targets) {
        DrawBodyOutline(hdc, &bodies[index], kGlowOuterColor, outer_width, game.cameraY);
    }
    for (const int index : targets) {
        DrawBodyOutline(hdc, &bodies[index], kGlowInnerColor, kGlowInnerWidth, game.cameraY);
    }
}

/** ステージと積んだ字、落とす前の字を描きます。 */
void DrawWorld(HDC hdc, S_GAME& game) {
    DrawBody(hdc, &bodies[stageIdx], kStageColor, game.cameraY);
    DrawGlows(hdc, game);

    for (int i = firstKanjiIdx; i < bodyCnt; ++i) {
        BODY* body = &bodies[i];
        DrawBody(hdc, body, body->IsResting() ? DimColor(body->GetRGB()) : body->GetRGB(), game.cameraY);
    }

    if (game.hasCurKanji) {
        DrawBody(hdc, &game.curKanji, game.curKanji.GetRGB(), game.cameraY);
    }
}

/** 右の欄に、スコアと、落とす字の候補を描きます。 */
void DrawPanel(HDC hdc, const S_GAME& game) {
    wchar_t text[64];
    int y = 20;

    swprintf_s(text, L"総画数: %d", game.strokeCnt);
    DrawString(hdc, kPanelLeft, y, text, kTextColor);
    y += kLineHeight;

    swprintf_s(text, L"高さ: %.1f字分  字数: %d", game.towerHeight / kanjiHeight, game.kanjiCnt);
    DrawString(hdc, kPanelLeft, y, text, kTextColor);
    y += kLineHeight * 2;

    for (int i = 0; i < handCnt; ++i) {
        if (i == game.selIdx) {
            FillBox(hdc, kPanelLeft - 4, y, kPanelLeft + 160, y + kHandHeight - 4, kSelectColor);
        }

        swprintf_s(text, L"%d", i + 1);
        DrawString(hdc, kPanelLeft + 4, y + 35, text, kTextColor);
        DrawKanji(hdc, game.hand[i], VEC2(kPanelLeft + 80.0f, y + 45.0f), 1.0f);
        y += kHandHeight;
    }
}

// ---------------------------------------------------------------------------
// リザルト画面
// ---------------------------------------------------------------------------

// 結果を書く枠と、その中の並べ方です。
constexpr RECT kResultBox = {220, 90, 740, 610};
constexpr int kResultBoxRadius = 20;
constexpr int kHeadingTop = kResultBox.top + 24;
constexpr int kScoreLabelTop = kResultBox.top + 96;
constexpr int kScoreTop = kResultBox.top + 118;
constexpr int kBestTop = kResultBox.top + 214;
constexpr int kDetailTop = kResultBox.top + 252;
constexpr int kMadeLabelTop = kResultBox.top + 296;
constexpr int kMadeTop = kResultBox.top + 350;

// 作った字の並べ方です。字の形で、1行に kMadePerRow 個ずつ並べます。
constexpr int kMadePerRow = 9;
constexpr int kMadeGap = 50;
constexpr float kMadeScale = 0.5f;

// ボタンは、枠の下の方に横に並べます。
constexpr int kResultButtonTop = 530;
constexpr int kResultButtonWidth = 220;
constexpr int kResultButtonHeight = 56;
constexpr int kResultButtonGap = 24;

/** プレイ画面の上に重ねる白の濃さ(0〜255)です。 */
constexpr int kPlayVeil = 150;

constexpr UINT kGameOverColor = RGB(190, 50, 40);
constexpr UINT kNewBestColor = RGB(30, 120, 70);

/** リザルト画面のボタンです。メニューの番号の順に並べます。 */
struct ResultItem {
    ResultButton button;
    const wchar_t* text;
};

constexpr ResultItem kResultItems[] = {
    {ResultButton::kRetry, L"リトライ"},
    {ResultButton::kTitle, L"タイトルへ"},
};

Menu result_menu;

ResultButton ToResultButton(int menu_index) {
    return menu_index >= 0 ? kResultItems[menu_index].button : ResultButton::kNone;
}

/** スコアと自己ベスト、塔の高さと字数を描きます。 */
void DrawScore(HDC hdc, const S_GAME& game) {
    const int center_x = kScreenWidth / 2;
    wchar_t text[64];

    DrawCenteredText(hdc, center_x, kHeadingTop, L"ゲームオーバー", 48, kGameOverColor);
    DrawCenteredText(hdc, center_x, kScoreLabelTop, L"総画数", 22, kDimTextColor);

    swprintf_s(text, L"%d", game.strokeCnt);
    DrawCenteredText(hdc, center_x, kScoreTop, text, 88, kTextColor);

    if (game.isNewBest) {
        DrawCenteredText(hdc, center_x, kBestTop, L"自己ベスト更新!", 22, kNewBestColor);
    } else {
        swprintf_s(text, L"自己ベスト: %d", game.bestScore);
        DrawCenteredText(hdc, center_x, kBestTop, text, 22, kDimTextColor);
    }

    swprintf_s(text, L"高さ: %.1f字分    字数: %d", game.towerHeight / kanjiHeight, game.kanjiCnt);
    DrawCenteredText(hdc, center_x, kDetailTop, text, 20, kTextColor);
}

/** このゲームで作った字を、字の形で並べて描きます。行ごとに、横の真ん中にそろえます。 */
void DrawMadeKanji(HDC hdc, const S_GAME& game) {
    const int center_x = kScreenWidth / 2;
    DrawCenteredText(hdc, center_x, kMadeLabelTop, L"作った字", 20, kDimTextColor);

    std::vector<Kanji> made;
    for (int i = 0; i < kKanjiKindCount; ++i) {
        if (game.isMade[i]) {
            made.push_back(static_cast<Kanji>(i));
        }
    }

    if (made.empty()) {
        DrawCenteredText(hdc, center_x, kMadeTop - 10, L"なし", 20, kTextColor);
        return;
    }

    const int count = static_cast<int>(made.size());
    for (int i = 0; i < count; ++i) {
        const int row = i / kMadePerRow;
        const int count_in_row = (std::min)(kMadePerRow, count - row * kMadePerRow);
        const int x = center_x - (count_in_row - 1) * kMadeGap / 2 + (i % kMadePerRow) * kMadeGap;
        const int y = kMadeTop + row * kMadeGap;
        DrawKanji(hdc, made[i], VEC2(static_cast<float>(x), static_cast<float>(y)), kMadeScale);
    }
}

}  // namespace

void RenderPlayView(HDC hdc, bool is_paused) {
    S_GAME& game = *GetGame();

    FillBox(hdc, 0, 0, kScreenWidth, kScreenHeight, kBackgroundColor);
    DrawWorld(hdc, game);
    DrawPanel(hdc, game);

    collection_button.Render(hdc, is_collection_button_hovered);
    controls_image.Load(kControlsImage);
    controls_image.Render(hdc, kControlsLeft, kControlsTop);

    if (is_paused) {
        FillBox(hdc, kPauseBandBox.left, kPauseBandBox.top, kPauseBandBox.right, kPauseBandBox.bottom, kPauseBandColor);
        DrawCenteredText(hdc, kScreenWidth / 2, kPauseBandBox.top + 20, L"一時停止", 64, kPauseTextColor);
    }
}

void PlayViewMouseMove(int x, int y) {
    is_collection_button_hovered = collection_button.Contains(x, y);
}

bool PlayViewMouseDown(int x, int y) {
    return collection_button.Contains(x, y);
}

void OpenResultView() {
    is_collection_button_hovered = false;

    result_menu.Clear();
    int left = kScreenWidth / 2 - kResultButtonWidth - kResultButtonGap / 2;
    for (const ResultItem& item : kResultItems) {
        result_menu.Add(item.text, {left, kResultButtonTop, left + kResultButtonWidth, kResultButtonTop + kResultButtonHeight});
        left += kResultButtonWidth + kResultButtonGap;
    }
}

void RenderResultView(HDC hdc) {
    const S_GAME& game = *GetGame();
    const RECT screen = {0, 0, kScreenWidth, kScreenHeight};

    RenderPlayView(hdc, false);
    FillBoxAlpha(hdc, screen, kBackgroundColor, kPlayVeil);

    FillRoundBox(hdc, kResultBox, kResultBoxRadius, kBackgroundColor, kFrameColor);
    DrawScore(hdc, game);
    DrawMadeKanji(hdc, game);
    result_menu.Render(hdc);
}

ResultButton ResultViewKeyDown(int key) {
    return ToResultButton(result_menu.OnKeyDown(key));
}

void ResultViewMouseMove(int x, int y) {
    result_menu.OnMouseMove(x, y);
}

ResultButton ResultViewMouseDown(int x, int y) {
    return ToResultButton(result_menu.OnMouseDown(x, y));
}
