/* このファイルはAI(Claude)が書いたものです。 */
//字のエディタ
//10×10マスの上で画を引いて字の形を作り、データファイルに保存する
//作った字は試しに落として、立つか転がるかを見られる
#pragma once
#include <windows.h>

void OpenEditor();
void UpdateEditor();
void RenderEditor(HDC hdc);

void EditorKeyDown(int key);
void EditorMouseDown(int x, int y);
void EditorMouseMove(int x, int y);
void EditorMouseUp(int x, int y);
