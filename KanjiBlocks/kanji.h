/**
 * @file kanji.h
 * @brief 漢字の形と合体を扱います。
 *
 * 1字を1つの剛体として物理に入れます。画1本を太さのある線分とみなし、凸四角形の部品1つにします。
 * 字の形はデータファイル(kKanjiDataFile)から読み込めます。ファイルが無いときは組み込みの形を使います。
 *
 * このファイルは AI(Claude)が書いたものです。
 */
#pragma once

#include "physics.h"

/**
 * 字の種類です。
 * 前の7つが基本の字(落とす字の候補に出す字)で、後ろは合体してできる字です。
 */
enum class Kanji {
    kTree,     ///< 木
    kSun,      ///< 日
    kMoon,     ///< 月
    kPerson,   ///< 人
    kMouth,    ///< 口
    kWater,    ///< 水
    kFire,     ///< 火
    kWoods,    ///< 林 = 木 + 木
    kForest,   ///< 森 = 木 + 林
    kBright,   ///< 明 = 日 + 月
    kFriend,   ///< 朋 = 月 + 月
    kProsper,  ///< 昌 = 日 + 日
    kRest,     ///< 休 = 人(亻) + 木
    kTurn,     ///< 回 = 口 + 口
    kTrouble,  ///< 困 = 口 + 木
    kShoes,    ///< 沓 = 水 + 日
    kFlame,    ///< 炎 = 火 + 火
    kWander,   ///< 徊 = 人(彳) + 回
    kWash,     ///< 沐 = 水(氵) + 木
    kChant,    ///< 唱 = 口 + 昌
    kEast,     ///< 東 = 日 + 木
    kSparkle,  ///< 晶 = 日 + 昌
    kBurn,     ///< 焚 = 林 + 火
    kPrison,   ///< 囚 = 人 + 口
};

/** 字の種類の数です。 */
constexpr int kKanjiKindCount = 24;

/** 基本の字(落とす字の候補に出す字)の一覧です。 */
constexpr Kanji kBaseKanji[] = {
    Kanji::kTree, Kanji::kSun, Kanji::kMoon, Kanji::kPerson, Kanji::kMouth, Kanji::kWater, Kanji::kFire,
};

/** 基本の字の数です。 */
constexpr int kBaseKanjiCount = sizeof(kBaseKanji) / sizeof(kBaseKanji[0]);

/** 字の形を表すマス目の、1マスの大きさ(px)です。 */
constexpr float kKanjiUnit = 8.0f;

/** 1字のマス目の大きさ(マス)です。どの字も 10×10 マスです。 */
constexpr float kKanjiGrid = 10.0f;

/** 字の形のデータファイルの名前です。今の作業フォルダに置きます。 */
constexpr const char* kKanjiDataFile = "kanji_data.txt";

/** 物理に入れた字の記録を消します。ClearBodies と一緒に呼んでください。 */
void ResetKanji();

/**
 * 字の形を物体に入れ、質量を計算します。重心が物体の位置に来ます。
 * @param body 空の物体
 * @param kind 字の種類
 */
void BuildKanji(BODY* body, Kanji kind);

/**
 * 字を、重心が center に来るように物理に入れます。
 * @return 入れた物体。入れられなかったときは NULL です。
 */
BODY* PutKanji(Kanji kind, VEC2 center, float angle);

/**
 * 合体の表にある組み合わせの2字が触れていたら、1字に置き換えます(1フレームに1組まで)。
 * StepPhysics の後に毎フレーム呼んでください。
 * @param result 合体したときに、できた字が入ります。
 * @return 合体したら true です。
 */
bool MergeKanji(Kanji* result);

/**
 * 物体が何の字かを調べます。
 * @param body_index 物体の番号(bodies の何番目か)
 * @param kind 字だったときに、その種類が入ります。
 * @return 字なら true、字ではない物体(ステージなど)なら false です。
 */
bool GetBodyKanji(int body_index, Kanji* kind);

/** 字の名前(例: L"木")を返します。 */
const wchar_t* GetKanjiName(Kanji kind);

/** 字の画数を返します。 */
int GetKanjiStrokeCount(Kanji kind);

/** 字の色を返します。 */
UINT GetKanjiColor(Kanji kind);

/**
 * 合体してできる字の作り方を、説明の文(例: 「木+木 → 林」、偏を使う字は「水+木 → 沐(氵)」)にします。
 * @param result 合体してできる字
 * @param buffer 文を書き込む場所
 * @param buffer_length buffer に入る文字の数
 * @return result が合体してできる字なら true、基本の字なら false です。
 */
bool GetRecipeText(Kanji result, wchar_t* buffer, int buffer_length);

// ---------------------------------------------------------------------------
// 字の形の編集(エディタ用)
// ---------------------------------------------------------------------------

/** 画の端の作り方です。 */
enum class StrokeEnd {
    kKeep,    ///< そのままにします。
    kExtend,  ///< 太さの半分だけ伸ばします。角で、相手の画の外側の縁までそろえるときに使います。
    kTrim,    ///< 太さの半分だけ縮めます。相手の画の縁で止めるときに使います。
    kFlat,    ///< 端を通る水平な線で切ります。地面に平らに着く足にするときに使います。
};

/**
 * 画1本です。
 * 座標はマス単位で、左上が原点、下向きが正です。
 */
struct Stroke {
    VEC2 start;  ///< 中心線の始点です。
    VEC2 end;    ///< 中心線の終点です。
    StrokeEnd start_end = StrokeEnd::kKeep;  ///< 始点の端の作り方です。
    StrokeEnd end_end = StrokeEnd::kKeep;    ///< 終点の端の作り方です。
};

/** 編集できる形の数です。基本の字7つ(kBaseKanji の順)と、偏3つ(亻・彳・氵)です。 */
constexpr int kEditShapeCount = kBaseKanjiCount + 3;

/** 1つの形に置ける画の数の上限です。 */
constexpr int kMaxShapeStrokes = 12;

/** 画の長さの下限(マス)です。これより短い画は作れません。 */
constexpr float kMinStrokeLength = 0.3f;

/** 編集できる形の名前(例: L"木"、L"亻")を返します。 */
const wchar_t* GetEditShapeName(int shape);

/**
 * 編集できる形が字かどうかを調べます。
 * @param kind 字だったときに、その種類が入ります。
 * @return 字なら true、偏なら false です。
 */
bool GetEditShapeKanji(int shape, Kanji* kind);

/**
 * 形の今の画を写します。
 * @param out kMaxShapeStrokes 本ぶんの配列
 * @return 画の数
 */
int GetEditShapeStrokes(int shape, Stroke* out);

/**
 * 形の画を置き換え、合体してできる字も含めて形を作り直します。
 * 画が無い、短すぎる、合体してできる字の部品が多すぎる(maxPartCnt を超える)ときは、何も変えずに false を返します。
 */
bool SetEditShapeStrokes(int shape, const Stroke* strokes, int count);

/**
 * 画1本を部品にしたときの4つの頂点を求めます。
 * @param points 4つぶんの配列。マス目の左上を原点にした px で入ります。
 */
void MakeStrokePoints(const Stroke& stroke, VEC2* points);

/**
 * 編集できる形をすべてデータファイルに書きます。
 * @return 書けたら true です。
 */
bool SaveKanjiData();
