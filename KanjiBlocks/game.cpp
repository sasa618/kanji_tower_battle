//ゲーム本体
//宙に浮いた平らなステージに漢字を物理で積む
//合体の表にある組み合わせの字が触れると合体し、ステージから落ちたら終わり
#include <stdio.h>  //swprintf_s用
#include <stdlib.h>  //rand用
#include "game.h"
#include "physics.h"
#include "kanji.h"
#include "draw.h"
#include "editor.h"
#include "collection.h"

//ステージの物体の番号(最初に入れる)
const int stageIdx = 0;
//ステージより後ろに入れた物体が字
const int firstKanjiIdx = 1;

//落とす前の字を動かせる横の範囲(画面の左右の説明の欄にかからない所)
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

//落とす字の候補の数
//毎回ランダムに選ばれたこの数の字(どれも違う字)から、落とす字を自分で選ぶ
const int handCnt = 3;

//落とした字が着いてから次の字を出すまで、全部が止まるのを待つフレーム数の上限(約2秒)
const int maxSettleFrameCnt = 120;

//1字の高さ(px)
//塔の高さを何字分かで表すのに使う
const float kanjiHeight = kKanjiUnit * kKanjiGrid;
//右の欄に出す合体の例(簡単なものだけ)
const Kanji recipeExamples[] = {Kanji::kWoods, Kanji::kForest, Kanji::kRest, Kanji::kWash, Kanji::kBright, Kanji::kTurn};
const int recipeExampleCnt = sizeof(recipeExamples) / sizeof(recipeExamples[0]);
//合体してできた字を知らせる表示のフレーム数
const int madeFrameCnt = 90;

//右の欄と左の欄の位置、説明文の行の高さ
const int panelX = 780;
const int leftPanelX = 20;
const int lineHeight = 24;

//色
const UINT backgroundColor = RGB(255, 255, 255);
const UINT stageColor = RGB(120, 95, 70);
const UINT textColor = RGB(40, 40, 40);
const UINT selectColor = RGB(255, 240, 180);  //選んでいる候補の背景
const UINT gameOverColor = RGB(40, 40, 50);  //ゲームオーバーの帯
const UINT gameOverTextColor = RGB(255, 255, 255);

//今の画面(ゲーム、字のエディタ、漢字図鑑)
enum class SCREEN { GAME, EDITOR, COLLECTION };
static SCREEN screen = SCREEN::GAME;

//物理を進めているか
static bool isPlaying = false;
//ゲームオーバーになったか(字がステージから落ちた)
static bool isGameOver = false;

//スコア(ステージの上に積んだ字の総画数)
//補助として、塔の高さ(px)と字数も数える
//ゲームオーバーになったら、その時の値で止める
static int strokeCnt = 0;
static float towerHeight = 0.0f;
static int kanjiCnt = 0;

//落とす字の候補(ランダムに選ばれた字)と、今選んでいる候補の番号
//字を1つ落とすたびに配り直す
static Kanji hand[handCnt];
static int selIdx = 0;

//落とす前の字
//選んでいる候補の字
//まだ物理には入れず、塔の上で待たせる
//操作できるのはこの間だけ
static BODY curKanji;
static bool hasCurKanji = false;
static float waitBottom = defaultWaitBottom;  //待たせる高さ(字の下の端)

//落とした字の通し番号
//ステージか他の字に着くまで覚えておく
//なければ-1
static int fallingID = -1;
//落とした字が着いてからのフレーム数
static int settleFrames = 0;

//画面の上の端の高さ(塔が高くなると上へずらす)と、その目標
static float cameraY = 0.0f;
static float targetCameraY = 0.0f;

//合体してできた字と、知らせる表示の残りのフレーム数
static Kanji madeKind = Kanji::kTree;
static int madeFrames = 0;

//ステージを置く
static void MakeStage() {
	VEC2 center = VEC2((stageLeft + stageRight) / 2.0f, stageTop + stageThickness / 2.0f);

	AddWall(center, stageRight - stageLeft, stageThickness, stageColor);
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

	for(int i=0; i<handCnt; i++) hand[i] = kinds[i];
	selIdx = 0;
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
	curKanji.UpdateBoundingBox();
	VEC2 lt = curKanji.GetBoundingBoxLT();
	VEC2 rb = curKanji.GetBoundingBoxRB();

	float dx = 0.0f;
	if(lt.x < moveLeft) dx = moveLeft - lt.x;
	if(rb.x > moveRight) dx = moveRight - rb.x;

	curKanji.MoveCenterPos(VEC2(dx, waitBottom - rb.y));
}

//選んでいる候補の字を、落とす前の字にする(位置xと角度angleはそのまま)
static void SetCurKanji(float x, float angle) {
	Kanji kind = hand[selIdx];

	curKanji.Initialize(VEC2(x, 0.0f), angle, GetKanjiColor(kind));
	BuildKanji(&curKanji, kind);
	PlaceCurKanji();

	hasCurKanji = true;
}

//候補を選び直し、選んでいる候補の字をステージの真ん中の上に出して待たせる
//塔が高くなっていたら、待たせる高さと画面を上へずらす
static void SpawnKanji() {
	DealHand();

	waitBottom = min(defaultWaitBottom, GetTowerTop() - waitGap);
	targetCameraY = min(0.0f, waitBottom - defaultWaitBottom);

	SetCurKanji((stageLeft + stageRight) / 2.0f, 0.0f);
}

//候補をidx番目に選び直す
//落とす前の字を、その候補の字に取り替える
//位置と角度はそのまま
static void SelectHand(int idx) {
	if(!hasCurKanji) return;

	selIdx = idx;
	SetCurKanji(curKanji.GetCenter().x, curKanji.GetAngle());
}

//落とす前の字を横に動かす
static void MoveCurKanji(float dx) {
	if(!hasCurKanji) return;

	curKanji.MoveCenterPos(VEC2(dx, 0.0f));
	PlaceCurKanji();
}

//落とす前の字を90度回す
static void RotateCurKanji() {
	if(!hasCurKanji) return;

	curKanji.SetAngle(curKanji.GetAngle() + pi / 2.0f);
	PlaceCurKanji();
}

//落とす前の字を物理に入れて落とす
//ここから先は操作できない
static void DropCurKanji() {
	if(!hasCurKanji || isGameOver) return;

	BODY* body = PutKanji(hand[selIdx], curKanji.GetCenter(), curKanji.GetAngle());
	if(body == NULL) return;

	fallingID = body->GetID();
	hasCurKanji = false;

	RecordDropped(hand[selIdx]);  //漢字図鑑に記録
}

//落とした字がステージか他の字に着いたか(合体して無くなったときも、着いたことにする)
static bool IsFallingKanjiLanded() {
	int idx = FindBodyIdx(fallingID);
	if(idx < 0) return true;

	for(int i=0; i<GetContactCnt(); i++) {
		int a, b;
		GetContactPair(i, &a, &b);

		if(a == idx || b == idx) return true;
	}

	return false;
}

//字が1つでもステージから落ちていたら(重心がステージの下の面より下にあれば)、ゲームオーバーにする
static void CheckGameOver() {
	for(int i=firstKanjiIdx; i<bodyCnt; i++) {
		if(bodies[i].GetCenter().y <= stageTop + stageThickness) continue;

		isGameOver = true;
		hasCurKanji = false;
		return;
	}
}

//スコアを数え直す(ステージの上に積んだ字の総画数と、塔の高さ、字数)
//落とした後まだ着いていない字と、ステージから落ちた字は数えない
static void UpdateScore() {
	strokeCnt = 0;
	kanjiCnt = 0;
	float top = stageTop;

	for(int i=firstKanjiIdx; i<bodyCnt; i++) {
		BODY* body = &bodies[i];
		if(body->GetID() == fallingID) continue;
		if(body->GetCenter().y > stageTop + stageThickness) continue;

		Kanji kind;
		if(!GetBodyKanji(i, &kind)) continue;

		strokeCnt += GetKanjiStrokeCount(kind);
		kanjiCnt++;
		top = min(top, body->GetBoundingBoxLT().y);
	}

	towerHeight = stageTop - top;
}

//ゲームの進行

//初期状態に戻す(エディタや漢字図鑑を開いていたら閉じる)
void ResetGame() {
	screen = SCREEN::GAME;

	ClearBodies();
	ResetKanji();
	MakeStage();

	isPlaying = false;
	isGameOver = false;

	strokeCnt = 0;
	towerHeight = 0.0f;
	kanjiCnt = 0;
	madeFrames = 0;

	DealHand();

	hasCurKanji = false;
	waitBottom = defaultWaitBottom;
	fallingID = -1;
	settleFrames = 0;

	cameraY = 0.0f;
	targetCameraY = 0.0f;
}

//動かす/止めるを切り替える
void ToggleRun() {
	isPlaying = !isPlaying;
}

bool IsRunning() { return isPlaying; }

//ゲームの画面か(エディタや漢字図鑑を開いていないか)
bool IsGameScreen() { return screen == SCREEN::GAME; }

//1フレーム進める(1フレームの時間がたつごとに呼ばれる)
//ゲームオーバーの後も、落ちていく様子が見えるように物理は進める
void UpdateGame() {
	//エディタを開いているときはエディタを進め、漢字図鑑を開いているときはゲームを止めておく
	if(screen == SCREEN::EDITOR) {
		UpdateEditor();
		return;
	}
	if(screen == SCREEN::COLLECTION) return;

	if(!isPlaying) return;

	StepPhysics(frameTime);

	//字が触れたら、合体するか調べる
	//合体したら、できた字を知らせる
	Kanji made;
	if(MergeKanji(&made)) {
		madeKind = made;
		madeFrames = madeFrameCnt;

		RecordMade(made);  //漢字図鑑に記録
	}

	if(madeFrames > 0) madeFrames--;

	//落とした字が着いたら、全部が止まってから(長くてもmaxSettleFrameCnt後に)次の字を出す
	if(fallingID >= 0 && IsFallingKanjiLanded()) {
		fallingID = -1;
		settleFrames = 0;
	}

	settleFrames++;
	bool isSettled = IsAllResting() || settleFrames >= maxSettleFrameCnt;
	if(!isGameOver && !hasCurKanji && fallingID < 0 && isSettled) SpawnKanji();

	//スコアは、ゲームオーバーになった時の値で止める
	if(!isGameOver) {
		CheckGameOver();
		UpdateScore();
	}

	//画面を目標の高さへ少しずつずらす
	cameraY += (targetCameraY - cameraY) * cameraSpeed;
}

//キーを押したとき
//矢印キーと1～3は落とす前の字に効く
//落とした後は効かない
void OnKeyDown(int key) {
	//エディタ: EかEscで閉じ、ゲームを最初からにする(試しに落とすために物体を消しているため)
	if(screen == SCREEN::EDITOR) {
		if(key == 'E' || key == VK_ESCAPE) ResetGame();
		else EditorKeyDown(key);
		return;
	}

	//漢字図鑑: BかEscで閉じて、止めておいたゲームに戻る
	//矢印キーは説明を出す字を選ぶ
	if(screen == SCREEN::COLLECTION) {
		if(key == 'B' || key == VK_ESCAPE) screen = SCREEN::GAME;
		else CollectionKeyDown(key);
		return;
	}

	switch(key) {
		case VK_SPACE:  //開始/停止
			ToggleRun();
			break;
		case 'R':  //リスタート(最初からにして、すぐに動かす)
			ResetGame();
			isPlaying = true;
			break;
		case VK_LEFT:  //左へ
			MoveCurKanji(-moveStep);
			break;
		case VK_RIGHT:  //右へ
			MoveCurKanji(moveStep);
			break;
		case VK_UP:  //回す
			RotateCurKanji();
			break;
		case VK_DOWN:  //落とす
			DropCurKanji();
			break;
		case '1':  //候補を選ぶ
		case '2':
		case '3':
			SelectHand(key - '1');
			break;
		case 'E':  //字のエディタを開く
			screen = SCREEN::EDITOR;
			OpenEditor();
			break;
		case 'B':  //漢字図鑑を開く
			screen = SCREEN::COLLECTION;
			break;
		default:
			break;
	}
}

//マウスのボタンを押したとき・動かしたとき・離したとき(エディタを開いているときだけ使う)
void OnMouseDown(int x, int y) {
	if(screen == SCREEN::EDITOR) EditorMouseDown(x, y);
	if(screen == SCREEN::COLLECTION) CollectionMouseDown(x, y);
}

void OnMouseMove(int x, int y) {
	if(screen == SCREEN::EDITOR) EditorMouseMove(x, y);
}

void OnMouseUp(int x, int y) {
	if(screen == SCREEN::EDITOR) EditorMouseUp(x, y);
}

//描画

//静止中の物を少し暗くした色
static UINT DimColor(UINT rgb) {
	return RGB(GetRValue(rgb) * 8 / 10, GetGValue(rgb) * 8 / 10, GetBValue(rgb) * 8 / 10);
}

//物体を1つ描く
//部品ごとに多角形を描く
//枠線も同じ色にして、画の継ぎ目を見せない
//offsetY: 画面をずらした分(描く位置をこの分だけ下げる)
static void DrawBody(HDC hdc, BODY* body, UINT rgb, float offsetY) {
	VEC2 points[maxPointCnt];

	for(int p=0; p<body->GetPartCnt(); p++) {
		body->GetWorldPoints(p, points);
		for(int i=0; i<body->GetPointCnt(p); i++) points[i].y -= offsetY;

		FillPolygon(hdc, points, body->GetPointCnt(p), rgb, rgb);
	}
}

//字kindの見本を、重心がcenterに来るように描く(画面のずれは足さない)
static void DrawKanjiSample(HDC hdc, Kanji kind, VEC2 center) {
	BODY sample;
	sample.Initialize(center, 0.0f, GetKanjiColor(kind));
	BuildKanji(&sample, kind);

	DrawBody(hdc, &sample, sample.GetRGB(), 0.0f);
}

//説明文を1行描いて次の行へ
static void DrawTextLine(HDC hdc, int x, int* y, const wchar_t* text) {
	DrawString(hdc, x, *y, text, textColor);
	*y += lineHeight;
}

//文字を、横の真ん中がcenterXに来るように太字で描く(背景は塗らない)
//size: 文字の高さ(px)
static void DrawCenteredText(HDC hdc, int centerX, int y, const wchar_t* text, int size, UINT rgb) {
	HFONT hFont = CreateFontW(size, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Meiryo UI");
	HGDIOBJ hOldFont = SelectObject(hdc, hFont);

	SetBkMode(hdc, TRANSPARENT);
	SetTextColor(hdc, rgb);
	SetTextAlign(hdc, TA_CENTER | TA_TOP);

	TextOutW(hdc, centerX, y, text, lstrlenW(text));

	SetTextAlign(hdc, TA_LEFT | TA_TOP);
	SelectObject(hdc, hOldFont);
	DeleteObject(hFont);
}

//ゲームオーバーの表示(画面の真ん中を横切る帯に、スコアとやり直し方を書く)
static void DrawGameOver(HDC hdc) {
	int top = 250;
	int centerX = screenWidth / 2;

	FillBox(hdc, 0, top, screenWidth, top + 180, gameOverColor);

	DrawCenteredText(hdc, centerX, top + 20, L"ゲームオーバー", 64, gameOverTextColor);

	wchar_t text[64];
	swprintf_s(text, L"総画数: %d    高さ: %.1f字分    字数: %d", strokeCnt, towerHeight / kanjiHeight, kanjiCnt);
	DrawCenteredText(hdc, centerX, top + 100, text, 24, gameOverTextColor);
	DrawCenteredText(hdc, centerX, top + 140, L"R キーでリスタート", 20, gameOverTextColor);
}

//画面を描く
void RenderGame(HDC hdc) {
	//エディタと漢字図鑑は、それぞれの画面を描く
	if(screen == SCREEN::EDITOR) {
		RenderEditor(hdc);
		return;
	}
	if(screen == SCREEN::COLLECTION) {
		RenderCollection(hdc, screenWidth, screenHeight);
		return;
	}

	FillBox(hdc, 0, 0, screenWidth, screenHeight, backgroundColor);

	//ステージと積んだ字
	DrawBody(hdc, &bodies[stageIdx], stageColor, cameraY);

	for(int i=firstKanjiIdx; i<bodyCnt; i++) {
		BODY* body = &bodies[i];

		UINT rgb = body->GetRGB();
		if(body->IsResting()) rgb = DimColor(rgb);

		DrawBody(hdc, body, rgb, cameraY);
	}

	//落とす前の字
	if(hasCurKanji) DrawBody(hdc, &curKanji, curKanji.GetRGB(), cameraY);

	//右の欄: スコア、落とす字の候補、合体の例
	wchar_t text[64];
	int y = 20;

	swprintf_s(text, L"総画数: %d", strokeCnt);
	DrawTextLine(hdc, panelX, &y, text);
	swprintf_s(text, L"高さ: %.1f字分  字数: %d", towerHeight / kanjiHeight, kanjiCnt);
	DrawTextLine(hdc, panelX, &y, text);
	y += lineHeight / 2;

	//落とす字の候補(選んでいるものは背景を塗る)
	DrawTextLine(hdc, panelX, &y, L"落とす字(1～3で選んでください)");
	for(int i=0; i<handCnt; i++) {
		if(i == selIdx) FillBox(hdc, panelX - 4, y, panelX + 160, y + 90, selectColor);

		swprintf_s(text, L"%d", i + 1);
		DrawString(hdc, panelX + 4, y + 35, text, textColor);
		DrawKanjiSample(hdc, hand[i], VEC2((float)panelX + 80.0f, (float)y + 45.0f));

		y += 94;
	}
	y += lineHeight / 2;

	DrawTextLine(hdc, panelX, &y, L"合体の例");
	for(int i=0; i<recipeExampleCnt; i++) {
		GetRecipeText(recipeExamples[i], text, 64);
		DrawTextLine(hdc, panelX, &y, text);
	}

	y += lineHeight / 2;
	if(madeFrames > 0) {
		swprintf_s(text, L"%lsができた!(%d画)", GetKanjiName(madeKind), GetKanjiStrokeCount(madeKind));
		DrawTextLine(hdc, panelX, &y, text);
	}

	//左の欄: 操作の説明
	y = 150;
	DrawTextLine(hdc, leftPanelX, &y, L"Space: 開始/停止");
	DrawTextLine(hdc, leftPanelX, &y, L"←→: 動かす");
	DrawTextLine(hdc, leftPanelX, &y, L"↑: 回す");
	DrawTextLine(hdc, leftPanelX, &y, L"↓: 落とす");
	DrawTextLine(hdc, leftPanelX, &y, L"1～3: 字を選ぶ");
	DrawTextLine(hdc, leftPanelX, &y, L"R: リスタート");
	DrawTextLine(hdc, leftPanelX, &y, L"E: 字のエディタ");
	swprintf_s(text, L"B: 漢字図鑑(%d/%d)", CountFoundKanji(), kKanjiKindCount);
	DrawTextLine(hdc, leftPanelX, &y, text);

	y += lineHeight / 2;
	DrawTextLine(hdc, leftPanelX, &y, L"組み合わせの合う字が");
	DrawTextLine(hdc, leftPanelX, &y, L"触れると合体");
	DrawTextLine(hdc, leftPanelX, &y, L"ステージから");
	DrawTextLine(hdc, leftPanelX, &y, L"落ちたら終わり");
	DrawTextLine(hdc, leftPanelX, &y, L"積んだ字の総画数が");
	DrawTextLine(hdc, leftPanelX, &y, L"スコア");

	//ゲームオーバー(いちばん上に重ねる)
	if(isGameOver) DrawGameOver(hdc);
}
