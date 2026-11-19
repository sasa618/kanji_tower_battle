/**
 * @file record.h
 * @brief 自己ベスト(総画数)を扱います。
 *
 * 記録はファイル(kBestScoreFile)に保存するので、次に起動したときも残ります。
 *
 * このファイルは AI(Claude)が書いたものです。
 */
#pragma once

/** 自己ベストのファイルの名前です。今の作業フォルダに置きます。 */
constexpr const char* kBestScoreFile = "kanji_best.txt";

/** 自己ベストを返します。まだ記録が無ければ 0 です。 */
int GetBestScore();

/**
 * スコアが自己ベストを超えていたら、自己ベストを書き換えてファイルに保存します。
 * @return 書き換えたら true です。
 */
bool UpdateBestScore(int score);
