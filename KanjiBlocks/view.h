/**
 * @file view.h
 * @brief 画面ごとの表示(レイアウトと描画、ボタン)です。
 *
 * どの画面に移るかや、キーでゲームをどう動かすかは、画面のクラス(scene_*.cpp)の側で決めます。
 * ここでは、何をどこに描くかと、どのボタンが押されたかだけを扱います。
 *
 * このファイルは AI(Claude)が書いたものです。
 */
#pragma once

#include <windows.h>

// ---------------------------------------------------------------------------
// タイトル画面: 背景にプレイ映像を上下2段に並べて薄く流し、スタート・遊び方・終了のボタンを出します。
// ---------------------------------------------------------------------------

/** タイトル画面のボタンです。 */
enum class TitleButton {
    kNone,   ///< どのボタンも決めていません。
    kStart,  ///< スタート
    kHowto,  ///< 遊び方
    kQuit,   ///< 終了
};

/** タイトル画面を開いたときに呼びます。ボタンを並べ、プレイ映像を開きます。 */
void OpenTitleView();

/** 1フレーム進めます。プレイ映像を進めて、横に流します。 */
void UpdateTitleView();

void RenderTitleView(HDC hdc);

/** @return 決めたボタンです。 */
TitleButton TitleViewKeyDown(int key);

void TitleViewMouseMove(int x, int y);

/** @return 押したボタンです。 */
TitleButton TitleViewMouseDown(int x, int y);

// ---------------------------------------------------------------------------
// 遊び方画面: 説明の画像を1枚貼り、戻るボタンを出します。
// ---------------------------------------------------------------------------

/** 遊び方画面を開いたときに呼びます。 */
void OpenHowtoView();

void RenderHowtoView(HDC hdc);

/** @return 戻るボタンを決めたら true です。 */
bool HowtoViewKeyDown(int key);

/** @return 戻るボタンを押したら true です。 */
bool HowtoViewMouseDown(int x, int y);

// ---------------------------------------------------------------------------
// プレイ画面: ステージと字、スコア、落とす字の候補、操作の説明の画像、漢字図鑑のボタンを出します。
// ---------------------------------------------------------------------------

/**
 * プレイ画面を描きます。落とす前の字と合体できる字は、黄色く光らせます。
 * @param is_paused 一時停止していれば true です。画面の真ん中に帯を出します。
 */
void RenderPlayView(HDC hdc, bool is_paused);

void PlayViewMouseMove(int x, int y);

/** @return 漢字図鑑のボタンを押したら true です。 */
bool PlayViewMouseDown(int x, int y);

// ---------------------------------------------------------------------------
// リザルト画面: プレイ画面の上に、結果とリトライ・タイトルへのボタンを重ねて出します。
// ---------------------------------------------------------------------------

/** リザルト画面のボタンです。 */
enum class ResultButton {
    kNone,   ///< どのボタンも決めていません。
    kRetry,  ///< リトライ
    kTitle,  ///< タイトルへ
};

/** リザルト画面を開いたときに呼びます。 */
void OpenResultView();

void RenderResultView(HDC hdc);

/** @return 決めたボタンです。 */
ResultButton ResultViewKeyDown(int key);

void ResultViewMouseMove(int x, int y);

/** @return 押したボタンです。 */
ResultButton ResultViewMouseDown(int x, int y);
