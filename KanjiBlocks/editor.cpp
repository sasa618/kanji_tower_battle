/* このファイルはAI(Claude)が書いたものです。 */
//字のエディタ
//10×10マスの上で画を引いて字の形を作り、データファイルに保存する
//作った字は試しに落として、立つか転がるかを見られる
//形を変えるたびに、合体してできる字も含めてゲームの字の形を作り直す
#include <stdio.h>  //swprintf_s用
#include <math.h>  //floorf、fmodf用
#include "editor.h"
#include "game.h"
#include "physics.h"
#include "kanji.h"
#include "draw.h"

//画を引くマス目(左上の位置と、1マスの大きさ)
const int canvasLeft = 40;
const int canvasTop = 60;
const float cellSize = 40.0f;
const int canvasSize = 400;  //10マス分

//マウスの位置をそろえる細かさ(マス)
const float snapStep = 0.1f;
//画の端をつかめる距離(マス)
const float grabDist = 0.35f;
//画を選べる距離(マス)
//中心線からこれより近ければ、その画を選ぶ
const float pickDist = 0.8f;

//試しに落とす所(枠と、ステージ)
const int testLeft = 480;
const int testRight = 940;
const int testTop = 60;
const int testBottom = 330;
const float testStageLeft = 560.0f;
const float testStageRight = 860.0f;
const float testStageTop = 300.0f;
const float testStageThickness = 12.0f;
//試しに落とす字の重心の位置
const VEC2 testDropPos = VEC2(710.0f, 140.0f);
//傾けて落とすときの角度(ラジアン、約17度)
const float testTiltAngle = 0.3f;

//説明の欄の位置と、行の高さ
const int helpX = 480;
const int helpY = 345;
const int lineHeight = 22;

//仕上がりの見本(全部の字)を並べる位置と、1行に並べる数
//全部の字が2行に収まるように、縮めて描く
const int sampleTop = 570;
const int sampleGap = 76;
const int sampleRowGap = 85;
const int sampleColumnCnt = 12;
const float sampleScale = 0.75f;

//知らせる文を出しておくフレーム数
const int messageFrameCnt = 120;

//色
const UINT backgroundColor = RGB(255, 255, 255);
const UINT gridColor = RGB(225, 225, 230);
const UINT frameColor = RGB(150, 150, 160);
const UINT baseLineColor = RGB(230, 120, 120);  //10マス目(足をそろえる線)
const UINT strokeColor = RGB(60, 120, 80);
const UINT selectedColor = RGB(240, 140, 30);
const UINT newStrokeColor = RGB(120, 170, 230);
const UINT startPointColor = RGB(40, 90, 220);
const UINT endPointColor = RGB(220, 50, 50);
const UINT stageColor = RGB(120, 95, 70);
const UINT textColor = RGB(40, 40, 40);
const UINT errorColor = RGB(210, 40, 40);

//ドラッグしているもの
enum class DRAG { NONE, NEW_STROKE, START_POINT, END_POINT };

//編集している形の番号(GetEditShapeNameの番号)と、その画
static int shapeIdx = 0;
static Stroke strokes[kMaxShapeStrokes];
static int strokeCnt = 0;

//選んでいる画(なければ-1)
static int selIdx = -1;

//1つ前の画(元に戻す用)
static Stroke prevStrokes[kMaxShapeStrokes];
static int prevStrokeCnt = 0;
static bool hasPrev = false;

//ドラッグ
static DRAG drag = DRAG::NONE;
static int dragIdx = -1;  //端を動かしている画
static VEC2 dragFrom;  //押した位置(マス)
static VEC2 dragTo;  //今の位置(マス)

//知らせる文と、表示の残りのフレーム数
static wchar_t message[80];
static int messageFrames = 0;
static bool isErrorMessage = false;

//試しに落とした字があるか(物理の物体0番がステージ、1番がその字)
static bool hasTest = false;

//知らせる文を出す
static void ShowMessage(const wchar_t* text, bool isError) {
	swprintf_s(message, L"%ls", text);
	messageFrames = messageFrameCnt;
	isErrorMessage = isError;
}

//座標の変換

//画面の位置を、マス目の位置にする(snapStepにそろえ、マス目の中に収める)
static VEC2 ToGrid(int x, int y) {
	float gx = (x - canvasLeft) / cellSize;
	float gy = (y - canvasTop) / cellSize;

	gx = floorf(gx / snapStep + 0.5f) * snapStep;
	gy = floorf(gy / snapStep + 0.5f) * snapStep;

	gx = max(0.0f, min(gx, kKanjiGrid));
	gy = max(0.0f, min(gy, kKanjiGrid));

	return VEC2(gx, gy);
}

//マス目の位置を、画面の位置にする
static VEC2 ToCanvas(VEC2 p) {
	return VEC2(canvasLeft + p.x * cellSize, canvasTop + p.y * cellSize);
}

//画面の位置が、マス目の中か
static bool IsInCanvas(int x, int y) {
	return x >= canvasLeft && x <= canvasLeft + canvasSize && y >= canvasTop && y <= canvasTop + canvasSize;
}

//画の編集

//編集する形の画を、今のゲームの形から読み込む
static void LoadShape() {
	strokeCnt = GetEditShapeStrokes(shapeIdx, strokes);
	selIdx = -1;
	hasPrev = false;
	drag = DRAG::NONE;
}

//画を置き換えて、ゲームの字の形を作り直す
//作れなければ元のままにして知らせる
//戻り値: 置き換えられたらtrue
static bool ApplyStrokes(const Stroke* newStrokes, int cnt) {
	Stroke backup[kMaxShapeStrokes];
	int backupCnt = strokeCnt;
	for(int i=0; i<strokeCnt; i++) backup[i] = strokes[i];

	if(!SetEditShapeStrokes(shapeIdx, newStrokes, cnt)) {
		ShowMessage(L"この形にはできません(画が無い・短すぎる・多すぎる)", true);
		return false;
	}

	for(int i=0; i<cnt; i++) strokes[i] = newStrokes[i];
	strokeCnt = cnt;

	//元に戻す用に、変える前の画を覚えておく
	for(int i=0; i<backupCnt; i++) prevStrokes[i] = backup[i];
	prevStrokeCnt = backupCnt;
	hasPrev = true;

	return true;
}

//点pから線分abまでの距離
static float GetSegmentDist(VEC2 p, VEC2 a, VEC2 b) {
	VEC2 ab = b - a;
	float len2 = Dot(ab, ab);
	if(len2 == 0.0f) return (p - a).Length();

	float t = Dot(p - a, ab) / len2;
	t = max(0.0f, min(t, 1.0f));

	return (p - (a + ab * t)).Length();
}

//点pのいちばん近くにある画(pickDistより遠ければ-1)
static int FindStrokeAt(VEC2 p) {
	int found = -1;
	float bestDist = pickDist;

	for(int i=0; i<strokeCnt; i++) {
		float dist = GetSegmentDist(p, strokes[i].start, strokes[i].end);
		if(dist >= bestDist) continue;

		bestDist = dist;
		found = i;
	}

	return found;
}

//点pの近くにある画の端(grabDistより遠ければ-1)
//選んでいる画の端を先に探す
//isStart: 見つかった端が始点ならtrue
static int FindEndPointAt(VEC2 p, bool* isStart) {
	for(int k=-1; k<strokeCnt; k++) {
		int i = (k < 0) ? selIdx : k;
		if(i < 0) continue;

		if((strokes[i].start - p).Length() < grabDist) {
			*isStart = true;
			return i;
		}
		if((strokes[i].end - p).Length() < grabDist) {
			*isStart = false;
			return i;
		}
	}

	return -1;
}

//端の作り方を次のものにする(そのまま → 伸ばす → 縮める → 平らな足 → そのまま)
static StrokeEnd GetNextEnd(StrokeEnd type) {
	switch(type) {
		case StrokeEnd::kKeep:
			return StrokeEnd::kExtend;
		case StrokeEnd::kExtend:
			return StrokeEnd::kTrim;
		case StrokeEnd::kTrim:
			return StrokeEnd::kFlat;
		case StrokeEnd::kFlat:
			return StrokeEnd::kKeep;
		default:
			break;
	}

	return StrokeEnd::kKeep;
}

//端の作り方の名前
static const wchar_t* GetEndName(StrokeEnd type) {
	switch(type) {
		case StrokeEnd::kKeep:
			return L"そのまま";
		case StrokeEnd::kExtend:
			return L"伸ばす";
		case StrokeEnd::kTrim:
			return L"縮める";
		case StrokeEnd::kFlat:
			return L"平らな足";
		default:
			break;
	}

	return L"";
}

//選んでいる画の始点か終点の端の作り方を、次のものにする
static void ChangeSelectedEnd(bool isStart) {
	if(selIdx < 0) {
		ShowMessage(L"先に画をクリックして選んでください", true);
		return;
	}

	Stroke tmp[kMaxShapeStrokes];
	for(int i=0; i<strokeCnt; i++) tmp[i] = strokes[i];

	if(isStart) tmp[selIdx].start_end = GetNextEnd(tmp[selIdx].start_end);
	else tmp[selIdx].end_end = GetNextEnd(tmp[selIdx].end_end);

	ApplyStrokes(tmp, strokeCnt);
}

//選んでいる画を消す
static void DeleteSelected() {
	if(selIdx < 0) return;

	Stroke tmp[kMaxShapeStrokes];
	int cnt = 0;
	for(int i=0; i<strokeCnt; i++) {
		if(i == selIdx) continue;

		tmp[cnt] = strokes[i];
		cnt++;
	}

	if(ApplyStrokes(tmp, cnt)) selIdx = -1;
}

//1つ前の画に戻す(もう一度押すと、戻す前に戻る)
static void Undo() {
	if(!hasPrev) return;

	Stroke tmp[kMaxShapeStrokes];
	int cnt = prevStrokeCnt;
	for(int i=0; i<cnt; i++) tmp[i] = prevStrokes[i];

	if(ApplyStrokes(tmp, cnt)) selIdx = -1;
}

//編集する形を切り替える(dir: 1なら次、-1なら前)
static void ChangeShape(int dir) {
	shapeIdx = (shapeIdx + dir + kEditShapeCount) % kEditShapeCount;
	LoadShape();
}

//試しに落とす

//編集している字を、試しに落とす所のステージの上から落とす(偏は字ではないので落とせない)
static void DropTest(float angle) {
	Kanji kind;
	if(!GetEditShapeKanji(shapeIdx, &kind)) {
		ShowMessage(L"偏は字ではないので落とせません(休・徊・沐で見てください)", true);
		return;
	}

	ClearBodies();
	ResetKanji();

	VEC2 stageCenter = VEC2((testStageLeft + testStageRight) / 2.0f, testStageTop + testStageThickness / 2.0f);
	AddWall(stageCenter, testStageRight - testStageLeft, testStageThickness, stageColor);
	PutKanji(kind, testDropPos, angle);

	hasTest = true;
}

//試しに落とした字の傾き(度、-180～180)
static float GetTestAngle() {
	float deg = fmodf(bodies[1].GetAngle(), 2.0f * pi) * 180.0f / pi;
	if(deg > 180.0f) deg -= 360.0f;
	if(deg < -180.0f) deg += 360.0f;

	return deg;
}

//エディタの進行

//エディタを開く(試しに落とす所のために、物理の物体は全部消す)
void OpenEditor() {
	ClearBodies();
	ResetKanji();
	hasTest = false;
	messageFrames = 0;

	LoadShape();
}

//1フレーム進める(試しに落とした字を動かす)
void UpdateEditor() {
	if(hasTest) StepPhysics(frameTime);
	if(messageFrames > 0) messageFrames--;
}

//キーを押したとき(EとEscはゲームの側で受けて、エディタを閉じる)
void EditorKeyDown(int key) {
	switch(key) {
		case VK_LEFT:  //前の形
			ChangeShape(-1);
			break;
		case VK_RIGHT:  //次の形
			ChangeShape(1);
			break;
		case VK_DELETE:  //選んだ画を消す
		case VK_BACK:
			DeleteSelected();
			break;
		case 'Q':  //始点の端の作り方を変える
			ChangeSelectedEnd(true);
			break;
		case 'W':  //終点の端の作り方を変える
			ChangeSelectedEnd(false);
			break;
		case 'Z':  //元に戻す
			Undo();
			break;
		case 'T':  //まっすぐ落とす
			DropTest(0.0f);
			break;
		case 'G':  //傾けて落とす
			DropTest(testTiltAngle);
			break;
		case 'S':  //保存
			if(SaveKanjiData()) ShowMessage(L"kanji_data.txt に保存しました", false);
			else ShowMessage(L"保存できませんでした", true);
			break;
		default:
			break;
	}
}

//マウスのボタンを押したとき
//画の端の近くならその端をつかみ、それ以外なら新しい画を引き始める
void EditorMouseDown(int x, int y) {
	if(!IsInCanvas(x, y)) return;

	VEC2 p = ToGrid(x, y);
	dragFrom = p;
	dragTo = p;

	bool isStart;
	int idx = FindEndPointAt(p, &isStart);
	if(idx >= 0) {
		drag = isStart ? DRAG::START_POINT : DRAG::END_POINT;
		dragIdx = idx;
		selIdx = idx;
		return;
	}

	drag = DRAG::NEW_STROKE;
}

//マウスを動かしたとき(ドラッグ中なら、今の位置を覚える)
void EditorMouseMove(int x, int y) {
	if(drag == DRAG::NONE) return;

	dragTo = ToGrid(x, y);
}

//マウスのボタンを離したとき
//新しい画を足すか、つかんだ端を動かす
//ほとんど動かしていなければ、画を選ぶ
void EditorMouseUp(int x, int y) {
	if(drag == DRAG::NONE) return;

	dragTo = ToGrid(x, y);
	DRAG done = drag;
	drag = DRAG::NONE;

	Stroke tmp[kMaxShapeStrokes];
	for(int i=0; i<strokeCnt; i++) tmp[i] = strokes[i];

	//新しい画(短ければクリックとみなして、画を選ぶ)
	if(done == DRAG::NEW_STROKE) {
		if((dragTo - dragFrom).Length() < kMinStrokeLength) {
			selIdx = FindStrokeAt(dragFrom);
			return;
		}

		if(strokeCnt >= kMaxShapeStrokes) {
			ShowMessage(L"画が多すぎます", true);
			return;
		}

		tmp[strokeCnt].start = dragFrom;
		tmp[strokeCnt].end = dragTo;
		tmp[strokeCnt].start_end = StrokeEnd::kKeep;
		tmp[strokeCnt].end_end = StrokeEnd::kKeep;

		if(ApplyStrokes(tmp, strokeCnt + 1)) selIdx = strokeCnt - 1;
		return;
	}

	//つかんだ端を動かす
	if(done == DRAG::START_POINT) tmp[dragIdx].start = dragTo;
	else tmp[dragIdx].end = dragTo;

	ApplyStrokes(tmp, strokeCnt);
}

//描画

//1pxの縦線か横線を引く(長方形を塗る)
static void DrawLine(HDC hdc, int left, int top, int right, int bottom, UINT rgb) {
	FillBox(hdc, left, top, max(right, left + 1), max(bottom, top + 1), rgb);
}

//枠を描く
static void DrawFrame(HDC hdc, int left, int top, int right, int bottom, UINT rgb) {
	DrawLine(hdc, left, top, right, top, rgb);
	DrawLine(hdc, left, bottom, right, bottom, rgb);
	DrawLine(hdc, left, top, left, bottom, rgb);
	DrawLine(hdc, right, top, right, bottom, rgb);
}

//画の端の点を、小さな四角で描く
static void DrawPoint(HDC hdc, VEC2 p, int size, UINT rgb) {
	VEC2 pos = ToCanvas(p);
	int x = (int)pos.x;
	int y = (int)pos.y;

	FillBox(hdc, x - size, y - size, x + size + 1, y + size + 1, rgb);
}

//画1本を、マス目の上に描く(ゲームと同じ部品の形を、マス目の大きさに広げる)
static void DrawStroke(HDC hdc, const Stroke& stroke, UINT rgb) {
	VEC2 points[4];
	MakeStrokePoints(stroke, points);

	for(int i=0; i<4; i++) points[i] = VEC2(canvasLeft + points[i].x * cellSize / kKanjiUnit, canvasTop + points[i].y * cellSize / kKanjiUnit);

	FillPolygon(hdc, points, 4, rgb, rgb);
}

//物体を1つ描く(部品ごとに多角形を描く)
static void DrawBody(HDC hdc, BODY* body, UINT rgb) {
	VEC2 points[maxPointCnt];

	for(int p=0; p<body->GetPartCnt(); p++) {
		body->GetWorldPoints(p, points);
		FillPolygon(hdc, points, body->GetPointCnt(p), rgb, rgb);
	}
}

//字kindの見本を、重心がcenterに来るようにscale倍で描く
static void DrawKanjiSample(HDC hdc, Kanji kind, VEC2 center, float scale) {
	BODY sample;
	sample.Initialize(VEC2(0.0f, 0.0f), 0.0f, GetKanjiColor(kind));
	BuildKanji(&sample, kind);

	VEC2 points[maxPointCnt];
	for(int p=0; p<sample.GetPartCnt(); p++) {
		sample.GetWorldPoints(p, points);
		for(int i=0; i<sample.GetPointCnt(p); i++) points[i] = center + points[i] * scale;

		FillPolygon(hdc, points, sample.GetPointCnt(p), sample.GetRGB(), sample.GetRGB());
	}
}

//マス目と、編集している形の画
static void DrawCanvas(HDC hdc) {
	//マス目(1マスごとの線と、足をそろえる10マス目の線)
	for(int i=0; i<=(int)kKanjiGrid; i++) {
		int pos = (int)(i * cellSize);
		DrawLine(hdc, canvasLeft + pos, canvasTop, canvasLeft + pos, canvasTop + canvasSize, gridColor);
		DrawLine(hdc, canvasLeft, canvasTop + pos, canvasLeft + canvasSize, canvasTop + pos, gridColor);
	}

	FillBox(hdc, canvasLeft, canvasTop + canvasSize - 1, canvasLeft + canvasSize + 1, canvasTop + canvasSize + 2, baseLineColor);

	//画(つかんだ端を動かしている間は、動かした後の形で描く)
	for(int i=0; i<strokeCnt; i++) {
		Stroke s = strokes[i];
		if(i == dragIdx && drag == DRAG::START_POINT) s.start = dragTo;
		if(i == dragIdx && drag == DRAG::END_POINT) s.end = dragTo;

		DrawStroke(hdc, s, (i == selIdx) ? selectedColor : strokeColor);
	}

	//引いている途中の新しい画
	if(drag == DRAG::NEW_STROKE && (dragTo - dragFrom).Length() >= kMinStrokeLength) {
		Stroke s;
		s.start = dragFrom;
		s.end = dragTo;

		DrawStroke(hdc, s, newStrokeColor);
	}

	//画の端の点
	//始点は青、終点は赤
	//選んでいる画は大きく
	for(int i=0; i<strokeCnt; i++) {
		int size = (i == selIdx) ? 5 : 3;

		DrawPoint(hdc, strokes[i].start, size, startPointColor);
		DrawPoint(hdc, strokes[i].end, size, endPointColor);
	}
}

//試しに落とす所
static void DrawTest(HDC hdc) {
	DrawFrame(hdc, testLeft, testTop, testRight, testBottom, frameColor);
	DrawString(hdc, testLeft + 8, testTop + 6, L"試しに落とす(T: まっすぐ  G: 傾けて)", textColor);

	if(!hasTest) {
		VEC2 lt = VEC2(testStageLeft, testStageTop);
		FillBox(hdc, (int)lt.x, (int)lt.y, (int)testStageRight, (int)(testStageTop + testStageThickness), stageColor);
		return;
	}

	for(int i=0; i<bodyCnt; i++) DrawBody(hdc, &bodies[i], bodies[i].GetRGB());

	//試しに落とした字の傾きと、止まったか
	if(bodyCnt < 2) return;

	wchar_t text[64];
	float deg = GetTestAngle();
	const wchar_t* state = L"動いている";
	if(bodies[1].IsResting()) state = (fabsf(deg) < 5.0f) ? L"立った" : L"倒れた";

	swprintf_s(text, L"傾き: %.0f度  %ls", deg, state);
	DrawString(hdc, testLeft + 8, testBottom - 26, text, textColor);
}

//説明文を1行描いて次の行へ
static void DrawTextLine(HDC hdc, int x, int* y, const wchar_t* text) {
	DrawString(hdc, x, *y, text, textColor);
	*y += lineHeight;
}

//画面を描く
void RenderEditor(HDC hdc) {
	FillBox(hdc, 0, 0, screenWidth, screenHeight, backgroundColor);

	wchar_t text[96];

	//編集している形
	swprintf_s(text, L"字のエディタ: %ls  (%d / %d、←→で切り替え)", GetEditShapeName(shapeIdx), shapeIdx + 1, kEditShapeCount);
	DrawString(hdc, canvasLeft, 24, text, textColor);

	DrawCanvas(hdc);

	//選んでいる画の端の作り方
	int y = canvasTop + canvasSize + 10;
	if(selIdx >= 0) {
		swprintf_s(text, L"選んだ画: 始点 %ls (Q) / 終点 %ls (W)", GetEndName(strokes[selIdx].start_end), GetEndName(strokes[selIdx].end_end));
	}
	else {
		swprintf_s(text, L"画の数: %d  (画をクリックすると選べます)", strokeCnt);
	}
	DrawString(hdc, canvasLeft, y, text, textColor);

	//知らせる文
	if(messageFrames > 0) DrawString(hdc, canvasLeft, y + 26, message, isErrorMessage ? errorColor : textColor);

	DrawTest(hdc);

	//操作の説明
	y = helpY;
	DrawTextLine(hdc, helpX, &y, L"ドラッグ: 画を引く");
	DrawTextLine(hdc, helpX, &y, L"端の点をドラッグ: 端を動かす");
	DrawTextLine(hdc, helpX, &y, L"クリック: 画を選ぶ");
	DrawTextLine(hdc, helpX, &y, L"Delete: 選んだ画を消す   Z: 元に戻す");
	DrawTextLine(hdc, helpX, &y, L"Q / W: 始点 / 終点の端の作り方を変える");
	DrawTextLine(hdc, helpX, &y, L"S: kanji_data.txt に保存");
	DrawTextLine(hdc, helpX, &y, L"E / Esc: ゲームに戻る");
	DrawTextLine(hdc, helpX, &y, L"赤い線(10マス目)に足をそろえると立ちやすい");

	//仕上がりの見本(合体してできる字も、今の形で作り直したもの)
	DrawString(hdc, canvasLeft, sampleTop - 24, L"仕上がり(合体してできる字も含む)", textColor);
	for(int i=0; i<kKanjiKindCount; i++) {
		float cx = canvasLeft + 40.0f + (i % sampleColumnCnt) * sampleGap;
		float cy = sampleTop + 45.0f + (i / sampleColumnCnt) * sampleRowGap;

		DrawKanjiSample(hdc, (Kanji)i, VEC2(cx, cy), sampleScale);
	}
}
