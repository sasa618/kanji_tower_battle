//ゲームのルール(描画と入力の処理は持たない)
//宙に浮いた平らなステージに、物理演算で漢字を積み上げる
//合体の表にある組み合わせの字が触れると合体し、ステージから落ちたら終わり
#include <stdlib.h>  //rand用
#include "game.h"
#include "config.h"
#include "collection.h"
#include "record.h"

//落とす前の字を動かせる横の範囲(画面の左右の欄にかからない所)
const float moveLeft = 190.0f;
const float moveRight = 770.0f;
//落とす前の字を←→で1回に動かす長さ(px)
const float moveStep = kKanjiUnit;

//落とす前の字を待たせる高さ(字の下の端)
//塔が低いうちはこの高さ
const float defaultWaitBottom = 200.0f;
//塔が高くなったら、塔のいちばん上からこの長さだけ上で待たせる
const float waitGap = 160.0f;
//画面をずらす速さ(1フレームで目標までの残りのこの割合だけ動かす)
const float cameraSpeed = 0.1f;

//落とした字が着いてから次の字を出すまで、全部が止まるのを待つフレーム数の上限(約2秒)
const int maxSettleFrameCnt = 120;

//ゲームの状態
static S_GAME game;

S_GAME* GetGame() { return &game; }

//ステージを置く
static void MakeStage() {
	VEC2 center = VEC2((stageLeft + stageRight) / 2.0f, stageTop + stageThickness / 2.0f);

	AddWall(center, stageRight - stageLeft, stageThickness, kStageColor);
}

//候補を選び直す(基本の字から、どれも違う字をhandCnt個ランダムに選ぶ)
//基本の字を写して偏りなく並べ替え(後ろから順に、自分を含む手前のどれかと入れ替える)、先頭から使う
static void DealHand() {
	Kanji kinds[kBaseKanjiCount];
	for(int i=0; i<kBaseKanjiCount; i++) kinds[i] = kBaseKanji[i];

	for(int i=kBaseKanjiCount - 1; i>0; i--) {
		int j = rand() % (i + 1);

		Kanji tmp = kinds[i];
		kinds[i] = kinds[j];
		kinds[j] = tmp;
	}

	for(int i=0; i<handCnt; i++) game.hand[i] = kinds[i];
	game.selIdx = 0;
}

//積んだ字のいちばん上の高さ(字がなければステージの上の面)
static float GetTowerTop() {
	float top = stageTop;

	for(int i=firstKanjiIdx; i<bodyCnt; i++) top = min(top, bodies[i].GetBoundingBoxLT().y);

	return top;
}

//全部の字が止まっているか
static bool IsAllResting() {
	for(int i=firstKanjiIdx; i<bodyCnt; i++) {
		if(!bodies[i].IsResting()) return false;
	}

	return true;
}

//落とす前の字の操作(落としたら操作できない)

//落とす前の字を待つ位置に合わせる
//横は動かせる範囲からはみ出さないようにし、縦は字の下の端を待たせる高さにそろえる
static void PlaceCurKanji() {
	game.curKanji.UpdateBoundingBox();
	VEC2 lt = game.curKanji.GetBoundingBoxLT();
	VEC2 rb = game.curKanji.GetBoundingBoxRB();

	float dx = 0.0f;
	if(lt.x < moveLeft) dx = moveLeft - lt.x;
	if(rb.x > moveRight) dx = moveRight - rb.x;

	game.curKanji.MoveCenterPos(VEC2(dx, game.waitBottom - rb.y));
}

//選んでいる候補の字を、落とす前の字にする(位置xと角度angleはそのまま)
static void SetCurKanji(float x, float angle) {
	Kanji kind = game.hand[game.selIdx];

	game.curKanji.Initialize(VEC2(x, 0.0f), angle, GetKanjiColor(kind));
	BuildKanji(&game.curKanji, kind);
	PlaceCurKanji();

	game.hasCurKanji = true;
}

//候補を選び直し、選んでいる候補の字をステージの真ん中の上に出して待たせる
//塔が高くなっていたら、待たせる高さと画面を上へずらす
static void SpawnKanji() {
	DealHand();

	game.waitBottom = min(defaultWaitBottom, GetTowerTop() - waitGap);
	game.targetCameraY = min(0.0f, game.waitBottom - defaultWaitBottom);

	SetCurKanji((stageLeft + stageRight) / 2.0f, 0.0f);
}

//候補をidx番目に選び直す
//落とす前の字を、その候補の字に取り替える
//位置と角度はそのまま
void SelectHand(int idx) {
	if(!game.hasCurKanji) return;

	game.selIdx = idx;
	SetCurKanji(game.curKanji.GetCenter().x, game.curKanji.GetAngle());
}

//落とす前の字を横に動かす
//dir: 左は-1、右は1
void MoveCurKanji(int dir) {
	if(!game.hasCurKanji) return;

	game.curKanji.MoveCenterPos(VEC2(dir * moveStep, 0.0f));
	PlaceCurKanji();
}

//落とす前の字を90度回す
void RotateCurKanji() {
	if(!game.hasCurKanji) return;

	game.curKanji.SetAngle(game.curKanji.GetAngle() + pi / 2.0f);
	PlaceCurKanji();
}

//落とす前の字を物理演算の対象にして落とす
//ここから先は操作できない
void DropCurKanji() {
	if(!game.hasCurKanji || game.isGameOver) return;

	Kanji kind = game.hand[game.selIdx];

	BODY* body = PutKanji(kind, game.curKanji.GetCenter(), game.curKanji.GetAngle());
	if(body == NULL) return;

	game.fallingID = body->GetID();
	game.hasCurKanji = false;

	RecordDropped(kind);  //漢字図鑑に記録
}

//bodyIdx番目の物体が、落とす前の字と合体できる字か
bool CanMergeWithCurKanji(int bodyIdx) {
	if(!game.hasCurKanji) return false;

	Kanji kind;
	if(!GetBodyKanji(bodyIdx, &kind)) return false;

	return CanMergeKanji(game.hand[game.selIdx], kind);
}

//進行の判定

//落とした字がステージか他の字に着いたか(合体して無くなったときも、着いたことにする)
//接触点の物体の番号は合体で詰まる前のものなので、合体より先に呼ぶ
static bool IsFallingKanjiLanded() {
	int idx = FindBodyIdx(game.fallingID);
	if(idx < 0) return true;

	for(int i=0; i<GetContactCnt(); i++) {
		int a, b;
		GetContactPair(i, &a, &b);

		if(a == idx || b == idx) return true;
	}

	return false;
}

//字が触れていたら合体させる
//合体したら、できた字を覚えておく
static void CheckMerge() {
	Kanji made;
	if(!MergeKanji(&made)) return;

	game.isMade[(int)made] = true;

	RecordMade(made);  //漢字図鑑に記録
}

//字が1つでもステージから落ちているか(重心がステージの下の面より下にあるか)
static bool IsKanjiFell() {
	for(int i=firstKanjiIdx; i<bodyCnt; i++) {
		if(bodies[i].GetCenter().y > stageTop + stageThickness) return true;
	}

	return false;
}

//スコアを数え直す(ステージの上に積んだ字の総画数と、塔の高さ、字数)
//落とした後まだ着いていない字と、ステージから落ちた字は数えない
static void UpdateScore() {
	game.strokeCnt = 0;
	game.kanjiCnt = 0;
	float top = stageTop;

	for(int i=firstKanjiIdx; i<bodyCnt; i++) {
		BODY* body = &bodies[i];
		if(body->GetID() == game.fallingID) continue;
		if(body->GetCenter().y > stageTop + stageThickness) continue;

		Kanji kind;
		if(!GetBodyKanji(i, &kind)) continue;

		game.strokeCnt += GetKanjiStrokeCount(kind);
		game.kanjiCnt++;
		top = min(top, body->GetBoundingBoxLT().y);
	}

	game.towerHeight = stageTop - top;
}

//ゲームオーバーにする(スコアを確定して、自己ベストを書き換える)
static void EndGame() {
	game.isGameOver = true;
	game.hasCurKanji = false;

	game.isNewBest = UpdateBestScore(game.strokeCnt);
	game.bestScore = GetBestScore();
}

//ゲームの進行

//初期状態に戻す
void ResetGame() {
	ClearBodies();
	ResetKanji();
	MakeStage();

	game.isGameOver = false;
	game.frames = 0;

	game.strokeCnt = 0;
	game.towerHeight = 0.0f;
	game.kanjiCnt = 0;

	game.bestScore = GetBestScore();
	game.isNewBest = false;

	for(int i=0; i<kKanjiKindCount; i++) game.isMade[i] = false;

	DealHand();

	game.hasCurKanji = false;
	game.waitBottom = defaultWaitBottom;
	game.fallingID = -1;
	game.settleFrames = 0;

	game.cameraY = 0.0f;
	game.targetCameraY = 0.0f;
}

//1フレーム進める(1フレームの時間がたつごとに呼ばれる)
//ゲームオーバーの後も、落ちていく様子が見えるように物理は進める
void UpdateGame() {
	StepPhysics(kFrameTime);
	game.frames++;

	//落とした字が着いたら、全部が止まってから(長くてもmaxSettleFrameCnt後に)次の字を出す
	if(game.fallingID >= 0 && IsFallingKanjiLanded()) {
		game.fallingID = -1;
		game.settleFrames = 0;
	}

	CheckMerge();

	game.settleFrames++;
	bool isSettled = IsAllResting() || game.settleFrames >= maxSettleFrameCnt;
	if(!game.isGameOver && !game.hasCurKanji && game.fallingID < 0 && isSettled) SpawnKanji();

	//スコアは、ゲームオーバーになった時の値で止める
	if(!game.isGameOver) {
		bool isFell = IsKanjiFell();
		UpdateScore();

		if(isFell) EndGame();
	}

	//画面を目標の高さへ少しずつずらす
	game.cameraY += (game.targetCameraY - game.cameraY) * cameraSpeed;
}
