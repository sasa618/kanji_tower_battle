/**
 * @file kanji_shape.h
 * @brief 字の形のデータの型です。kanji_*.cpp の中だけで使います。
 *
 * 字の種類と形の番号、合体の表の1行、1字ぶんの形を定義します。
 * ゲームやエディタからは kanji.h を使ってください。
 *
 * このファイルは AI(Claude)が書いたものです。
 */
#pragma once

#include <array>
#include <vector>

#include "kanji.h"

namespace kanji_detail {

/**
 * 形の番号です。
 * 0 から kKanjiKindCount - 1 までは字そのもので、Kanji と同じ番号です。
 * その後ろは偏(合体してできる字の左側に使う形)と、エディタでは編集しない形です。
 */
using ShapeId = int;

constexpr ShapeId ShapeOf(Kanji kind) {
    return static_cast<int>(kind);
}

constexpr ShapeId kNinben = kKanjiKindCount;          ///< 亻(人偏)
constexpr ShapeId kGyouninben = kKanjiKindCount + 1;  ///< 彳(行人偏)
constexpr ShapeId kSanzui = kKanjiKindCount + 2;      ///< 氵(さんずい)
constexpr ShapeId kEastStrokes = kKanjiKindCount + 3; ///< 東の画(エディタでは編集しません)
constexpr int kShapeCount = kKanjiKindCount + 4;

constexpr ShapeId kTreeShape = ShapeOf(Kanji::kTree);
constexpr ShapeId kSunShape = ShapeOf(Kanji::kSun);
constexpr ShapeId kMoonShape = ShapeOf(Kanji::kMoon);
constexpr ShapeId kPersonShape = ShapeOf(Kanji::kPerson);
constexpr ShapeId kMouthShape = ShapeOf(Kanji::kMouth);
constexpr ShapeId kWaterShape = ShapeOf(Kanji::kWater);
constexpr ShapeId kFireShape = ShapeOf(Kanji::kFire);

using End = StrokeEnd;

/** 合体してできる字のマス目の中で、形を縮めて置く範囲(マス)です。 */
struct Box {
    float x;
    float y;
    float width;
    float height;
};

/** 合体してできる字に描く形と、その置き場所です。 */
struct Piece {
    ShapeId shape;
    Box box;
};

/**
 * 合体の表の1行です。a と b が触れると result になります(順不同)。
 * result の形は、pieces(1つから3つ)を並べたものです。東や晶のように、材料の字とは違う形を使っても構いません。
 *
 * 同じ組み合わせで作れる字がいくつかあるときは、転がりにくい形(底が平らで幅の広いもの)を選びます。
 * たとえば 木 + 口 は杏ではなく困にしています。
 * 後ろの行は、前の行でできる字を材料や形に使って構いません(森は林を、徊は回を、唱は昌を使います)。
 */
struct Recipe {
    Kanji result;
    Kanji a;
    Kanji b;
    std::vector<Piece> pieces;
};

extern const Recipe kRecipes[];

using StrokeList = std::vector<Stroke>;
using ShapeList = std::array<StrokeList, kShapeCount>;
using Quad = std::array<VEC2, 4>;

/** 1字ぶんの形です。座標はどちらも、マス目の左上を原点にした px です。 */
struct Form {
    std::vector<Quad> parts;  ///< 部品の頂点
    VEC2 centroid;            ///< 重心
};

/** 字の形と、合体の表の引き方をまとめたものです。 */
struct Tables {
    std::array<Form, kKanjiKindCount> forms;

    /** 2字の組み合わせから kRecipes の行の番号を引く表です。組み合わせが無いときは -1 です。 */
    int recipe_index[kKanjiKindCount][kKanjiKindCount];
};

/** 今使っている形です。最初に使うときに、組み込みの形とデータファイルから作ります。 */
struct ShapeData {
    std::array<StrokeList, kShapeCount> shapes;  ///< 基本の字と偏と東の画(合体してできる字の分は空)
    Tables tables;
};

/** 今使っている形を返します。最初に使うときに、組み込みの形とデータファイルから作ります。 */
ShapeData& GetShapeData();

/** 字の形と、合体の表の引き方を返します。 */
const Tables& GetTables();

/** 編集できる形の番号を、形の番号に変えます。 */
ShapeId EditShapeToShapeId(int edit_shape);

/** 形 form の部品を物体に足します。 */
void AddParts(BODY* body, const Form& form);

/**
 * データファイルにある形で shapes を置き換えます。ファイルに出てくる形だけを置き換えます。
 *
 * ファイルは1行に画1本で、「形の名前 始点x 始点y 終点x 終点y 始点の端 終点の端」と書きます。
 * # で始まる行と空行は読み飛ばします。
 */
void LoadDataFile(ShapeList* shapes);

}  // namespace kanji_detail
