//ゲーム本体
//宙に浮いた平らなステージに漢字を物理で積む
//合体の表にある組み合わせの字が触れると合体し、ステージから落ちたら終わり
#pragma once
#include <windows.h>

//画面の大きさ
const int screenWidth = 960;
const int screenHeight = 780;

//1フレームの時間(秒)
const float frameTime = 1.0f / 60.0f;

//ステージ(字を積む平らな台)
//横は基本の字5つ分
//塔が高くなると画面が上へずれるので、位置は画面がずれていないときの値
const float stageLeft = 280.0f;  //左の端
const float stageRight = 680.0f;  //右の端
const float stageTop = 600.0f;  //上の面
const float stageThickness = 16.0f;  //厚さ

void ResetGame();
void UpdateGame();
void RenderGame(HDC hdc);

void OnKeyDown(int key);
void OnMouseDown(int x, int y);
void OnMouseMove(int x, int y);
void OnMouseUp(int x, int y);

void ToggleRun();
bool IsRunning();
bool IsGameScreen();
