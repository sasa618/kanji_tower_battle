//ゲームのルール(描画と入力の処理は持たない)
//宙に浮いた平らなステージに、物理演算で漢字を積み上げる
//合体の表にある組み合わせの字が触れると合体し、ステージから落ちたら終わり
#pragma once
#include "kanji.h"
#include "physics.h"

//ステージ(字を積む平らな台)
//横は基本の字5つ分
//塔が高くなると画面が上へずれるので、位置は画面がずれていないときの値
const float stageLeft = 280.0f;  //左の端
const float stageRight = 680.0f;  //右の端
const float stageTop = 600.0f;  //上の面
const float stageThickness = 16.0f;  //厚さ

//ステージの物体の番号(最初に入れる)
const int stageIdx = 0;
//ステージより後ろに入れた物体が字
const int firstKanjiIdx = 1;

//落とす字の候補の数
//毎回ランダムに選ばれたこの数の字(どれも違う字)から、落とす字を自分で選ぶ
const int handCnt = 3;

//1字の高さ(px)
//塔の高さを何字分かで表すのに使う
const float kanjiHeight = kKanjiUnit * kKanjiGrid;

//ゲームの状態
struct S_GAME {
	bool isGameOver;  //ゲームオーバーになったか(字がステージから落ちた)
	int frames;  //始めてからのフレーム数

	//スコア(ステージの上に積んだ字の総画数)
	//補助として、塔の高さ(px)と字数も数える
	//ゲームオーバーになったら、その時の値で止める
	int strokeCnt;
	float towerHeight;
	int kanjiCnt;

	int bestScore;  //自己ベスト
	bool isNewBest;  //自己ベストを更新したか

	//落とす字の候補(ランダムに選ばれた字)と、今選んでいる候補の番号
	//字を1つ落とすたびに配り直す
	Kanji hand[handCnt];
	int selIdx;

	//落とす前の字
	//選んでいる候補の字
	//まだ物理演算の対象にはせず、塔の上で待たせる
	//操作できるのはこの間だけ
	BODY curKanji;
	bool hasCurKanji;
	float waitBottom;  //待たせる高さ(字の下の端)

	//落とした字の通し番号
	//ステージか他の字に着くまで覚えておく
	//なければ-1
	int fallingID;
	int settleFrames;  //落とした字が着いてからのフレーム数

	//画面の上の端の高さ(塔が高くなると上へずらす)と、その目標
	float cameraY;
	float targetCameraY;

	bool isMade[kKanjiKindCount];  //このゲームで合体して作った字か(リザルト画面に並べる)
};

S_GAME* GetGame();

void ResetGame();
void UpdateGame();

//落とす前の字の操作(落としたら操作できない)
void SelectHand(int idx);
void MoveCurKanji(int dir);
void RotateCurKanji();
void DropCurKanji();

bool CanMergeWithCurKanji(int bodyIdx);
