/**
 * @file collection.h
 * @brief 漢字図鑑(完成させた字のリスト)を扱います。
 *
 * 字ごとに漢字No.を付け、ゲームで落とした回数と、合体して作った回数を記録します。
 * 記録はファイル(kCollectionFile)に保存するので、次に起動したときも残ります。
 *
 * このファイルは AI(Claude)が書いたものです。
 */
#pragma once

#include <windows.h>

#include "kanji.h"

/** 図鑑の記録のファイルの名前です。今の作業フォルダに置きます。 */
constexpr const char* kCollectionFile = "kanji_collection.txt";

/** 漢字No.を返します。Kanji の順に 1 から数えます。 */
int GetKanjiNumber(Kanji kind);

/** 字を落としたことを記録し、ファイルに保存します。 */
void RecordDropped(Kanji kind);

/** 字を合体して作ったことを記録し、ファイルに保存します。 */
void RecordMade(Kanji kind);

/** 一度でも落としたか作ったことのある字の数を返します。 */
int CountFoundKanji();

/**
 * 図鑑の画面を描きます。上に字の一覧、下に選んでいる字の説明(読み・意味・熟語・作り方・作った回数)を出します。
 * @param width 画面の幅(px)
 * @param height 画面の高さ(px)
 */
void RenderCollection(HDC hdc, int width, int height);

/** 図鑑の画面でキーが押されたときに呼びます。矢印キーで選んでいる字を変えます。 */
void CollectionKeyDown(int key);

/** 図鑑の画面でクリックされたときに呼びます。クリックした字を選びます。 */
void CollectionMouseDown(int x, int y);
