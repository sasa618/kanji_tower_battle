/**
 * @file config.h
 * @brief 画面の大きさと時間の刻み、どの画面でも使う色です。
 *
 * このファイルは AI(Claude)が書いたものです。
 */
#pragma once

#include <windows.h>

/** 画面の大きさ(px)です。 */
constexpr int kScreenWidth = 960;
constexpr int kScreenHeight = 780;

/** 1フレームの時間(秒)です。 */
constexpr float kFrameTime = 1.0f / 60.0f;

constexpr UINT kBackgroundColor = RGB(255, 255, 255);
constexpr UINT kStageColor = RGB(120, 95, 70);
constexpr UINT kTextColor = RGB(40, 40, 40);
constexpr UINT kDimTextColor = RGB(120, 120, 130);

/** 選んでいる項目の背景の色です。 */
constexpr UINT kSelectColor = RGB(255, 240, 180);
