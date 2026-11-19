/**
 * @file editor.h
 * @brief 字のエディタです。
 *
 * 10×10 マスの上で画を引いて、基本の字と偏の形を作り、データファイル(kKanjiDataFile)に保存します。
 * 作った字は試しに落として、立つか倒れるかを確かめられます。
 *
 * このファイルは AI(Claude)が書いたものです。
 */
#pragma once

#include <windows.h>

/** エディタを開きます。試しに落とす枠で物理を使うので、物理の物体はすべて消します。 */
void OpenEditor();

/** 1フレーム進めます。試しに落とした字を動かします。 */
void UpdateEditor();

/** エディタの画面を描きます。 */
void RenderEditor(HDC hdc);

/** キーが押されたときに呼びます。E と Esc(エディタを閉じる)は画面の側で受けてください。 */
void EditorKeyDown(int key);

/** マウスのボタンが押されたときに呼びます。画の端の近くならその端をつかみ、それ以外なら新しい画を引き始めます。 */
void EditorMouseDown(int x, int y);

/** マウスが動いたときに呼びます。 */
void EditorMouseMove(int x, int y);

/** マウスのボタンが離されたときに呼びます。画を足すか端を動かします。ほとんど動かしていなければ画を選びます。 */
void EditorMouseUp(int x, int y);
