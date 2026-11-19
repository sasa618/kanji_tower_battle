/**
 * @file view_title.cpp
 * @brief タイトル画面と遊び方画面の表示の実装です。
 *
 * このファイルは AI(Claude)が書いたものです。
 */
#include "view.h"

#include <string>

#include "asset.h"
#include "config.h"
#include "draw.h"
#include "ui.h"

namespace {

// タイトル画面の並べ方です。
constexpr int kTitleTop = 170;
constexpr int kTitleTextSize = 88;
constexpr int kButtonLeft = 340;
constexpr int kButtonTop = 380;
constexpr int kButtonWidth = 280;
constexpr int kButtonHeight = 60;
constexpr int kButtonGap = 24;

/** タイトル画面のボタンです。メニューの番号の順に並べます。 */
struct TitleItem {
    TitleButton button;
    const wchar_t* text;
};

constexpr TitleItem kTitleItems[] = {
    {TitleButton::kStart, L"スタート"},
    {TitleButton::kHowto, L"遊び方"},
    {TitleButton::kQuit, L"終了"},
};

// 背景のプレイ映像の並べ方です。
// 上下2段に kMoviesPerRow 本ずつ横に並べ、上の段は右へ、下の段は左へ流します。
// data フォルダの play1.mp4 から順に、上の段の左から使います。
// 映像は、背景として邪魔にならないように、薄い色にして書き出してあります。
constexpr int kMovieRowCount = 2;
constexpr int kMoviesPerRow = 3;
constexpr int kMovieCount = kMovieRowCount * kMoviesPerRow;

/** プレイ映像1本の大きさ(px)です。横に並べた長さが、画面の幅より1本ぶん以上長くなるようにします。 */
constexpr int kMovieWidth = 480;
constexpr int kMovieHeight = 390;

/** プレイ映像を流す速さ(1フレームで動く長さ、px)です。 */
constexpr float kMovieScrollSpeed = 0.5f;

/** 遊び方の画像の名前と、戻るボタンの置き場所です。 */
constexpr const wchar_t* kHowtoImage = L"howto.bmp";
constexpr RECT kBackButtonBox = {380, 706, 580, 758};

Menu title_menu;
Movie title_movies[kMovieCount];
bool are_movies_open = false;
float movie_scroll = 0.0f;  ///< プレイ映像を流した長さ(px)

Image howto_image;
Menu howto_menu;

std::wstring MovieName(int index) {
    return L"play" + std::to_wstring(index + 1) + L".mp4";
}

/**
 * プレイ映像を上下2段に並べて描きます。
 * 1段ぶんの映像を横につないだ帯を、流した長さだけずらし、画面の外へ出た分は反対側から出します。
 */
void DrawMovies(HDC hdc) {
    const int band_width = kMovieWidth * kMoviesPerRow;
    const int scroll = static_cast<int>(movie_scroll) % band_width;

    for (int row = 0; row < kMovieRowCount; ++row) {
        const int direction = row % 2 == 0 ? 1 : -1;
        for (int column = 0; column < kMoviesPerRow; ++column) {
            const int offset = (column * kMovieWidth + direction * scroll + band_width) % band_width;
            title_movies[row * kMoviesPerRow + column].Render(hdc, offset - kMovieWidth, row * kMovieHeight);
        }
    }
}

TitleButton ToTitleButton(int menu_index) {
    return menu_index >= 0 ? kTitleItems[menu_index].button : TitleButton::kNone;
}

}  // namespace

void OpenTitleView() {
    title_menu.Clear();
    int top = kButtonTop;
    for (const TitleItem& item : kTitleItems) {
        title_menu.Add(item.text, {kButtonLeft, top, kButtonLeft + kButtonWidth, top + kButtonHeight});
        top += kButtonHeight + kButtonGap;
    }

    if (!are_movies_open) {
        for (int i = 0; i < kMovieCount; ++i) {
            title_movies[i].Open(MovieName(i));
        }
        are_movies_open = true;
    }
}

void UpdateTitleView() {
    // 最後まで流れた映像は、最初から流し直します。
    for (int i = 0; i < kMovieCount; ++i) {
        title_movies[i].Update(kFrameTime);
        if (title_movies[i].is_ended()) {
            title_movies[i].Open(MovieName(i));
        }
    }

    movie_scroll += kMovieScrollSpeed;
    if (movie_scroll >= kMovieWidth * kMoviesPerRow) {
        movie_scroll -= kMovieWidth * kMoviesPerRow;
    }
}

void RenderTitleView(HDC hdc) {
    FillBox(hdc, 0, 0, kScreenWidth, kScreenHeight, kBackgroundColor);

    DrawMovies(hdc);

    DrawCenteredText(hdc, kScreenWidth / 2, kTitleTop, L"漢字タワーバトル", kTitleTextSize, kTextColor);
    title_menu.Render(hdc);
}

TitleButton TitleViewKeyDown(int key) {
    return ToTitleButton(title_menu.OnKeyDown(key));
}

void TitleViewMouseMove(int x, int y) {
    title_menu.OnMouseMove(x, y);
}

TitleButton TitleViewMouseDown(int x, int y) {
    return ToTitleButton(title_menu.OnMouseDown(x, y));
}

void OpenHowtoView() {
    howto_image.Load(kHowtoImage);

    howto_menu.Clear();
    howto_menu.Add(L"戻る", kBackButtonBox);
}

void RenderHowtoView(HDC hdc) {
    FillBox(hdc, 0, 0, kScreenWidth, kScreenHeight, kBackgroundColor);
    howto_image.Render(hdc, 0, 0);
    howto_menu.Render(hdc);
}

bool HowtoViewKeyDown(int key) {
    return howto_menu.OnKeyDown(key) >= 0;
}

bool HowtoViewMouseDown(int x, int y) {
    return howto_menu.OnMouseDown(x, y) >= 0;
}
