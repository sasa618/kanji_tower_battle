/**
 * @file kanji.cpp
 * @brief 漢字の形と合体の実装です。
 *
 * 字の形について
 *   - 形は、画の中心線(始点と終点、マス単位)の並びで表します。どの字も 10×10 マスです。
 *   - 画1本を太さ kStrokeWidth の凸四角形にし、物体の部品1つにします。
 *   - 角や交わる所では、片方の画の端を太さの半分だけ伸ばすか縮めて、隙間も重なりも作りません。
 *     部品が重なると、重なった所の質量を二重に数えてしまうためです。
 *   - 地面に着く払いの先は水平に切って、平らな足にします。どの字も足の高さを10マス目にそろえます。
 *   - 合体してできる字も1字の大きさにします。材料の形(字そのものか、亻・彳・氵などの偏)の中心線を、
 *     決めた範囲に縮めて並べます。画の太さは変えません。
 *   - 基本の字と偏の形は、データファイルがあればそれで置き換えます。
 *     エディタで形を変えたときも、合体してできる字ごと作り直します。
 *
 * 合体について
 *   - 合体の表にある組み合わせの2字が触れたら、向きや並び方に関係なく1字に置き換えます。
 *   - 合体してできた字は、2字をひとまとまりの物体とみなしたときの速度と角速度を引き継ぎます。
 *
 * このファイルは AI(Claude)が書いたものです。
 */
#include "kanji.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

/** 画の太さ(マス)です。1マスが 8px なので 10px になります。 */
constexpr float kStrokeWidth = 1.25f;

int ToIndex(Kanji kind) {
    return static_cast<int>(kind);
}

/** 字ごとの名前・画数・色です。 */
struct KanjiInfo {
    const wchar_t* name;
    int stroke_count;  ///< 画数です。得点の計算に使います。
    UINT color;
};

/** Kanji の順に並べた字の情報です。 */
constexpr KanjiInfo kInfos[kKanjiKindCount] = {
    {L"木", 4, RGB(70, 175, 70)},
    {L"日", 4, RGB(245, 120, 30)},
    {L"月", 4, RGB(225, 175, 0)},
    {L"人", 2, RGB(200, 90, 160)},
    {L"口", 3, RGB(70, 110, 220)},
    {L"水", 4, RGB(30, 170, 210)},
    {L"火", 4, RGB(230, 70, 40)},
    {L"林", 8, RGB(35, 135, 60)},
    {L"森", 12, RGB(20, 95, 55)},
    {L"明", 8, RGB(190, 50, 60)},
    {L"朋", 8, RGB(170, 130, 0)},
    {L"昌", 8, RGB(200, 90, 20)},
    {L"休", 6, RGB(140, 80, 170)},
    {L"回", 6, RGB(40, 70, 170)},
    {L"困", 7, RGB(240, 150, 60)},
    {L"沓", 8, RGB(20, 120, 160)},
    {L"炎", 8, RGB(190, 30, 20)},
    {L"徊", 9, RGB(110, 60, 150)},
    {L"沐", 7, RGB(20, 140, 120)},
    {L"唱", 11, RGB(225, 95, 115)},
    {L"東", 8, RGB(110, 140, 30)},
    {L"晶", 12, RGB(170, 70, 30)},
    {L"焚", 12, RGB(160, 40, 80)},
    {L"囚", 5, RGB(90, 90, 110)},
};

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

/** 偏の名前です。kNinben から順に並べます。 */
constexpr const wchar_t* kRadicalNames[] = {L"亻", L"彳", L"氵"};

using End = StrokeEnd;

/** 組み込みの形の画1本です。データファイルが無いときに使います。 */
struct BuiltinStroke {
    ShapeId shape;
    Stroke stroke;
};

const BuiltinStroke kBuiltinStrokes[] = {
    // 木(4画): 縦画は横画のところで上下に分けます。払いの先は平らな足にし、縦画と合わせて3本の足で立たせます。
    {kTreeShape, {{1, 3}, {9, 3}}},
    {kTreeShape, {{5, 0}, {5, 3}, End::kKeep, End::kTrim}},
    {kTreeShape, {{5, 3}, {5, 10}, End::kTrim, End::kKeep}},
    {kTreeShape, {{4.5f, 4}, {1, 10}, End::kKeep, End::kFlat}},
    {kTreeShape, {{5.5f, 4}, {9, 10}, End::kKeep, End::kFlat}},

    // 日(4画): 外の囲みは、上下の横画と左右の縦画に分けます。
    {kSunShape, {{2, 1}, {8, 1}, End::kExtend, End::kExtend}},
    {kSunShape, {{2, 1}, {2, 9}, End::kTrim, End::kTrim}},
    {kSunShape, {{8, 1}, {8, 9}, End::kTrim, End::kTrim}},
    {kSunShape, {{2, 5}, {8, 5}, End::kTrim, End::kTrim}},
    {kSunShape, {{2, 9}, {8, 9}, End::kExtend, End::kExtend}},

    // 月(4画): 左の画は縦と払いに分けます。右下のはねは省きます。
    {kMoonShape, {{2, 1}, {2, 7.5f}, End::kExtend, End::kKeep}},
    {kMoonShape, {{2, 7}, {0.7f, 10}, End::kKeep, End::kFlat}},
    {kMoonShape, {{2, 1}, {8, 1}, End::kTrim, End::kTrim}},
    {kMoonShape, {{8, 1}, {8, 10}, End::kExtend, End::kKeep}},
    {kMoonShape, {{2, 4.25f}, {8, 4.25f}, End::kTrim, End::kTrim}},
    {kMoonShape, {{2, 7.5f}, {8, 7.5f}, End::kTrim, End::kTrim}},

    // 人(2画): 右の払いは、左の払いの中心線の途中(3分の1ほど下)から出します。2本の平らな足で立たせます。
    {kPersonShape, {{4.6f, 0.3f}, {1.2f, 10}, End::kKeep, End::kFlat}},
    {kPersonShape, {{3.38f, 3.79f}, {8.8f, 10}, End::kKeep, End::kFlat}},

    // 口(3画): 上の横画と右の縦画(合わせて1画)は分けます。
    {kMouthShape, {{2, 2}, {8, 2}, End::kExtend, End::kExtend}},
    {kMouthShape, {{2, 2}, {2, 9}, End::kTrim, End::kTrim}},
    {kMouthShape, {{8, 2}, {8, 9}, End::kTrim, End::kTrim}},
    {kMouthShape, {{2, 9}, {8, 9}, End::kExtend, End::kExtend}},

    // 水(4画): 真ん中の縦画のはねは省きます。左は横画と払い、右は短い払いと長い払いに分けます。
    {kWaterShape, {{5, 0.3f}, {5, 10}}},
    {kWaterShape, {{1.3f, 3.4f}, {4.4f, 3.4f}}},
    {kWaterShape, {{3.9f, 3.4f}, {1, 8.6f}}},
    {kWaterShape, {{8.4f, 2}, {5.6f, 4.6f}}},
    {kWaterShape, {{5.6f, 4}, {9, 10}, End::kKeep, End::kFlat}},

    // 火(4画): 真ん中の画は縦と払いに分けます。
    {kFireShape, {{2, 2.8f}, {3, 5}}},
    {kFireShape, {{8, 2.6f}, {7, 4.8f}}},
    {kFireShape, {{5, 0.3f}, {5, 5}}},
    {kFireShape, {{5, 5}, {1.2f, 10}, End::kKeep, End::kFlat}},
    {kFireShape, {{5, 5.4f}, {9, 10}, End::kKeep, End::kFlat}},

    // 亻(人偏): マス目の左側だけに描きます。
    {kNinben, {{3.4f, 0.3f}, {0.7f, 4.8f}}},
    {kNinben, {{2.1f, 2.8f}, {2.1f, 10}}},

    // 彳(行人偏)
    {kGyouninben, {{3.2f, 0.3f}, {0.7f, 2.9f}}},
    {kGyouninben, {{3.4f, 3}, {0.5f, 6.2f}}},
    {kGyouninben, {{2.1f, 4.9f}, {2.1f, 10}}},

    // 氵(さんずい): 下の画は左下から右上へはね上げます。
    {kSanzui, {{0.7f, 1}, {2.1f, 2.4f}}},
    {kSanzui, {{0.5f, 3.9f}, {1.9f, 5.3f}}},
    {kSanzui, {{0.6f, 10}, {2.9f, 6.3f}, End::kFlat, End::kKeep}},

    // 東(8画): 木と日を重ねると、木の払いが日の中を通ってしまうので、画を直接決めます。
    // 縦画は横画のところで分け、払いは日の下から出します。
    {kEastStrokes, {{1, 1.6f}, {9, 1.6f}}},
    {kEastStrokes, {{2.4f, 3.4f}, {7.6f, 3.4f}, End::kExtend, End::kExtend}},
    {kEastStrokes, {{2.4f, 3.4f}, {2.4f, 7.8f}, End::kTrim, End::kTrim}},
    {kEastStrokes, {{7.6f, 3.4f}, {7.6f, 7.8f}, End::kTrim, End::kTrim}},
    {kEastStrokes, {{2.4f, 5.6f}, {7.6f, 5.6f}, End::kTrim, End::kTrim}},
    {kEastStrokes, {{2.4f, 7.8f}, {7.6f, 7.8f}, End::kExtend, End::kExtend}},
    {kEastStrokes, {{5, 0}, {5, 1.6f}, End::kKeep, End::kTrim}},
    {kEastStrokes, {{5, 1.6f}, {5, 3.4f}, End::kTrim, End::kTrim}},
    {kEastStrokes, {{5, 3.4f}, {5, 5.6f}, End::kTrim, End::kTrim}},
    {kEastStrokes, {{5, 5.6f}, {5, 7.8f}, End::kTrim, End::kTrim}},
    {kEastStrokes, {{5, 7.8f}, {5, 10}, End::kTrim, End::kKeep}},
    {kEastStrokes, {{4.2f, 8.4f}, {1, 10}, End::kKeep, End::kFlat}},
    {kEastStrokes, {{5.8f, 8.4f}, {9, 10}, End::kKeep, End::kFlat}},
};

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

const Recipe kRecipes[] = {
    {Kanji::kWoods, Kanji::kTree, Kanji::kTree, {{kTreeShape, {-0.2f, 0, 5, 10}}, {kTreeShape, {5.2f, 0, 5, 10}}}},
    {Kanji::kForest, Kanji::kTree, Kanji::kWoods, {{kTreeShape, {1, 0, 8, 5}}, {ShapeOf(Kanji::kWoods), {0, 5, 10, 5}}}},
    {Kanji::kBright, Kanji::kSun, Kanji::kMoon, {{kSunShape, {0, 3, 4.2f, 7}}, {kMoonShape, {4.2f, 0, 5.8f, 10}}}},
    {Kanji::kFriend, Kanji::kMoon, Kanji::kMoon, {{kMoonShape, {-0.2f, 0, 5.2f, 10}}, {kMoonShape, {5, 0, 5.2f, 10}}}},
    {Kanji::kProsper, Kanji::kSun, Kanji::kSun, {{kSunShape, {-0.75f, 0, 11.5f, 4.6f}}, {kSunShape, {-1.5f, 5.2f, 13, 4.8f}}}},
    {Kanji::kRest, Kanji::kPerson, Kanji::kTree, {{kNinben, {0, 0, 10, 10}}, {kTreeShape, {3.4f, 0, 6.6f, 10}}}},
    {Kanji::kTurn, Kanji::kMouth, Kanji::kMouth, {{kMouthShape, {-1.25f, -1.875f, 12.5f, 12.5f}}, {kMouthShape, {2.5f, 2.25f, 5, 5}}}},
    {Kanji::kTrouble, Kanji::kMouth, Kanji::kTree, {{kMouthShape, {-1.25f, -1.875f, 12.5f, 12.5f}}, {kTreeShape, {2, 1.65f, 6, 7.1f}}}},
    {Kanji::kShoes, Kanji::kWater, Kanji::kSun, {{kWaterShape, {0, 0, 10, 5.5f}}, {kSunShape, {2, 5.2f, 6, 4.8f}}}},
    {Kanji::kFlame, Kanji::kFire, Kanji::kFire, {{kFireShape, {1, 0, 8, 5}}, {kFireShape, {0, 5, 10, 5}}}},
    {Kanji::kWander, Kanji::kPerson, Kanji::kTurn, {{kGyouninben, {0, 0, 10, 10}}, {ShapeOf(Kanji::kTurn), {3.6f, 0.6f, 6.4f, 8.8f}}}},
    {Kanji::kWash, Kanji::kWater, Kanji::kTree, {{kSanzui, {0, 0, 10, 10}}, {kTreeShape, {3.2f, 0, 6.8f, 10}}}},
    {Kanji::kChant, Kanji::kMouth, Kanji::kProsper, {{kMouthShape, {-0.2f, 2.6f, 4, 5.4f}}, {ShapeOf(Kanji::kProsper), {4.03f, 0, 6.35f, 10}}}},
    // 東は、専用の画(kEastStrokes)をそのまま使います。
    {Kanji::kEast, Kanji::kSun, Kanji::kTree, {{kEastStrokes, {0, 0, 10, 10}}}},
    // 晶は、昌ではなく日を3つ並べます(上に1つ、下に2つ)。
    {Kanji::kSparkle, Kanji::kSun, Kanji::kProsper,
     {{kSunShape, {2.2f, 0, 5.6f, 4.8f}}, {kSunShape, {-0.4f, 5.2f, 5.6f, 4.8f}}, {kSunShape, {4.8f, 5.2f, 5.6f, 4.8f}}}},
    {Kanji::kBurn, Kanji::kWoods, Kanji::kFire, {{ShapeOf(Kanji::kWoods), {0, 0, 10, 5}}, {kFireShape, {0.5f, 5.2f, 9, 4.8f}}}},
    {Kanji::kPrison, Kanji::kPerson, Kanji::kMouth, {{kMouthShape, {-1.25f, -1.875f, 12.5f, 12.5f}}, {kPersonShape, {2, 1.65f, 6, 7.1f}}}},
};

constexpr int kRecipeCount = sizeof(kRecipes) / sizeof(kRecipes[0]);

// ---------------------------------------------------------------------------
// 字の形
// ---------------------------------------------------------------------------

using StrokeList = std::vector<Stroke>;
using Quad = std::array<VEC2, 4>;

/** 1字ぶんの形です。座標はどちらも、マス目の左上を原点にした px です。 */
struct Form {
    std::vector<Quad> parts;  ///< 部品の頂点
    VEC2 centroid;            ///< 重心
};

/** 編集できる形の番号を、形の番号に変えます。 */
ShapeId EditShapeToShapeId(int edit_shape) {
    if (edit_shape < kBaseKanjiCount) {
        return ShapeOf(kBaseKanji[edit_shape]);
    }
    return kNinben + (edit_shape - kBaseKanjiCount);
}

/** 端を画の向きに動かす量を、太さの半分を 1 として返します。伸ばすときは正、縮めるときは負です。 */
float EndShift(End type) {
    switch (type) {
        case End::kExtend:
            return 1.0f;
        case End::kTrim:
            return -1.0f;
        default:
            return 0.0f;
    }
}

/**
 * 点 p を、画の向き direction に沿って高さ y まで動かします。平らな足の角を作るのに使います。
 * ほぼ水平な画は動かしません。
 */
VEC2 SlideToHeight(VEC2 p, VEC2 direction, float y) {
    if (std::fabs(direction.y) < 0.1f) {
        return p;
    }
    return p + direction * ((y - p.y) / direction.y);
}

/** 画1本を、太さ kStrokeWidth の凸四角形の部品にします。頂点は px で返します。 */
Quad MakeStrokeQuad(const Stroke& stroke) {
    const VEC2 direction = (stroke.end - stroke.start).Normalized();
    const VEC2 half_direction = direction * (kStrokeWidth / 2.0f);
    const VEC2 start = stroke.start - half_direction * EndShift(stroke.start_end);
    const VEC2 end = stroke.end + half_direction * EndShift(stroke.end_end);
    const VEC2 side = half_direction.Perpendicular();

    Quad quad = {start + side, end + side, end - side, start - side};

    if (stroke.start_end == End::kFlat) {
        quad[0] = SlideToHeight(quad[0], direction, start.y);
        quad[3] = SlideToHeight(quad[3], direction, start.y);
    }
    if (stroke.end_end == End::kFlat) {
        quad[1] = SlideToHeight(quad[1], direction, end.y);
        quad[2] = SlideToHeight(quad[2], direction, end.y);
    }

    for (VEC2& point : quad) {
        point *= kKanjiUnit;
    }
    return quad;
}

/** 1字のマス目の点 p を、範囲 box に縮めて置いた位置を返します。 */
VEC2 PlaceInBox(VEC2 p, const Box& box) {
    return VEC2(box.x + p.x * box.width / kKanjiGrid, box.y + p.y * box.height / kKanjiGrid);
}

/**
 * source の画を box に縮めて destination に足します。
 * 端の作り方はそのまま使います。太さは変えないので、継ぎ目は合ったままです。
 */
void AppendPlaced(StrokeList& destination, const StrokeList& source, const Box& box) {
    for (const Stroke& stroke : source) {
        destination.push_back({PlaceInBox(stroke.start, box), PlaceInBox(stroke.end, box), stroke.start_end, stroke.end_end});
    }
}

void AddParts(BODY* body, const Form& form) {
    for (const Quad& quad : form.parts) {
        body->AddPart(quad.data(), 4);
    }
}

/** 字の形と、合体の表の引き方をまとめたものです。 */
struct Tables {
    std::array<Form, kKanjiKindCount> forms;

    /** 2字の組み合わせから kRecipes の行の番号を引く表です。組み合わせが無いときは -1 です。 */
    int recipe_index[kKanjiKindCount][kKanjiKindCount];
};

/**
 * 基本の字と偏の形(shapes)から、合体してできる字も含めたすべての字の形を作ります。
 * @return どれかの字の部品が0個か、maxPartCnt を超えるときは false です。
 */
bool BuildTables(const std::array<StrokeList, kShapeCount>& shapes, Tables* tables) {
    std::array<StrokeList, kShapeCount> lines = shapes;

    // 表の順に作るので、材料の形は先にできています。
    for (const Recipe& recipe : kRecipes) {
        StrokeList& destination = lines[ShapeOf(recipe.result)];
        destination.clear();
        for (const Piece& piece : recipe.pieces) {
            AppendPlaced(destination, lines[piece.shape], piece.box);
        }
    }

    // 画を部品にし、重心は物理と同じ計算で求めます。
    for (int kind = 0; kind < kKanjiKindCount; ++kind) {
        const int part_count = static_cast<int>(lines[kind].size());
        if (part_count == 0 || part_count > maxPartCnt) {
            return false;
        }

        Form& form = tables->forms[kind];
        form.parts.clear();
        for (const Stroke& stroke : lines[kind]) {
            form.parts.push_back(MakeStrokeQuad(stroke));
        }

        BODY probe;
        probe.Initialize(VEC2(), 0.0f, 0);
        AddParts(&probe, form);
        form.centroid = probe.CalculateMass();
    }

    // 合体の表は順不同なので、両方の向きに入れます。
    for (auto& row : tables->recipe_index) {
        for (int& index : row) {
            index = -1;
        }
    }
    for (int i = 0; i < kRecipeCount; ++i) {
        const int a = ToIndex(kRecipes[i].a);
        const int b = ToIndex(kRecipes[i].b);
        tables->recipe_index[a][b] = i;
        tables->recipe_index[b][a] = i;
    }

    return true;
}

// ---------------------------------------------------------------------------
// データファイル
// ---------------------------------------------------------------------------

/** 端の作り方と、データファイルでの書き方の対応です。 */
struct EndName {
    End type;
    const char* text;
};

constexpr EndName kEndNames[] = {
    {End::kKeep, "keep"},
    {End::kExtend, "extend"},
    {End::kTrim, "trim"},
    {End::kFlat, "flat"},
};

const char* EndToText(End type) {
    for (const EndName& name : kEndNames) {
        if (name.type == type) {
            return name.text;
        }
    }
    return "keep";
}

std::optional<End> TextToEnd(const std::string& text) {
    for (const EndName& name : kEndNames) {
        if (text == name.text) {
            return name.type;
        }
    }
    return std::nullopt;
}

std::string ToUtf8(const wchar_t* text) {
    char buffer[16] = {};
    WideCharToMultiByte(CP_UTF8, 0, text, -1, buffer, sizeof(buffer), nullptr, nullptr);
    return buffer;
}

/** 名前(UTF-8)から、編集できる形の番号を探します。 */
std::optional<int> FindEditShape(const std::string& name) {
    for (int i = 0; i < kEditShapeCount; ++i) {
        if (ToUtf8(GetEditShapeName(i)) == name) {
            return i;
        }
    }
    return std::nullopt;
}

/**
 * データファイルにある形で shapes を置き換えます。ファイルに出てくる形だけを置き換えます。
 *
 * ファイルは1行に画1本で、「形の名前 始点x 始点y 終点x 終点y 始点の端 終点の端」と書きます。
 * # で始まる行と空行は読み飛ばします。
 */
void LoadDataFile(std::array<StrokeList, kShapeCount>* shapes) {
    std::ifstream file(kKanjiDataFile, std::ios::binary);
    if (!file) {
        return;
    }

    std::array<bool, kShapeCount> replaced = {};
    std::string line;

    while (std::getline(file, line)) {
        // UTF-8 の BOM と、行末の \r を取り除きます。
        if (line.size() >= 3 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) {
            line.erase(0, 3);
        }
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::istringstream in(line);
        std::string name;
        std::string start_text;
        std::string end_text;
        Stroke stroke;
        if (!(in >> name >> stroke.start.x >> stroke.start.y >> stroke.end.x >> stroke.end.y >> start_text >> end_text)) {
            continue;
        }

        const std::optional<int> edit_shape = FindEditShape(name);
        const std::optional<End> start_end = TextToEnd(start_text);
        const std::optional<End> end_end = TextToEnd(end_text);
        if (!edit_shape || !start_end || !end_end) {
            continue;
        }

        stroke.start_end = *start_end;
        stroke.end_end = *end_end;
        if ((stroke.end - stroke.start).Length() < kMinStrokeLength) {
            continue;
        }

        const ShapeId shape = EditShapeToShapeId(*edit_shape);
        if (!replaced[shape]) {
            (*shapes)[shape].clear();
            replaced[shape] = true;
        }
        if (static_cast<int>((*shapes)[shape].size()) < kMaxShapeStrokes) {
            (*shapes)[shape].push_back(stroke);
        }
    }
}

/** 今使っている形です。最初に使うときに、組み込みの形とデータファイルから作ります。 */
struct ShapeData {
    std::array<StrokeList, kShapeCount> shapes;  ///< 基本の字と偏と東の画(合体してできる字の分は空)
    Tables tables;
};

ShapeData MakeShapeData() {
    ShapeData data;
    for (const BuiltinStroke& builtin : kBuiltinStrokes) {
        data.shapes[builtin.shape].push_back(builtin.stroke);
    }

    // データファイルの形で作れないときは、組み込みの形に戻します。
    const std::array<StrokeList, kShapeCount> builtin_shapes = data.shapes;
    LoadDataFile(&data.shapes);
    if (!BuildTables(data.shapes, &data.tables)) {
        data.shapes = builtin_shapes;
        BuildTables(data.shapes, &data.tables);
    }

    return data;
}

ShapeData& GetShapeData() {
    static ShapeData data = MakeShapeData();
    return data;
}

const Tables& GetTables() {
    return GetShapeData().tables;
}

/** 物理に入れた字の記録です。物体の通し番号から字の種類を引きます。 */
std::unordered_map<int, Kanji> kind_by_body_id;

std::optional<Kanji> KindOf(int body_index) {
    const auto found = kind_by_body_id.find(bodies[body_index].GetID());
    if (found == kind_by_body_id.end()) {
        return std::nullopt;
    }
    return found->second;
}

// ---------------------------------------------------------------------------
// 合体
// ---------------------------------------------------------------------------

/**
 * 物体 index_a と index_b を、合体してできる字1つに置き換えます。
 *
 * 位置は2字の重心(質量で重みづけ)、向きは2字の向きを質量で重みづけした平均にします。
 * 速度は、2字をひとまとまりの物体とみなしたときの重心の速度にします。
 *
 * 角速度は 0 にします。
 * 2字の角運動量を引き継ぐと、まっすぐ落として合体させても、少しずれて当たっただけで回り出して不自然に見えるためです。
 */
void Fuse(int index_a, int index_b, Kanji result) {
    // 消す前に写しておきます。
    BODY a = bodies[index_a];
    BODY b = bodies[index_b];

    const float mass_a = 1.0f / a.GetInvMass();
    const float mass_b = 1.0f / b.GetInvMass();
    const float total_mass = mass_a + mass_b;

    const VEC2 center = (a.GetCenter() * mass_a + b.GetCenter() * mass_b) / total_mass;
    const VEC2 velocity = (a.GetVelocity() * mass_a + b.GetVelocity() * mass_b) / total_mass;
    const float angle = std::atan2(std::sin(a.GetAngle()) * mass_a + std::sin(b.GetAngle()) * mass_b,
                                   std::cos(a.GetAngle()) * mass_a + std::cos(b.GetAngle()) * mass_b);

    // 後ろにある方から消すと、前にある方の番号がずれません。
    kind_by_body_id.erase(a.GetID());
    kind_by_body_id.erase(b.GetID());
    RemoveBody(index_a > index_b ? index_a : index_b);
    RemoveBody(index_a > index_b ? index_b : index_a);

    BODY* body = PutKanji(result, center, angle);
    if (body == nullptr) {
        return;
    }

    body->SetVelocity(velocity);
    body->SetAngularVelocity(0.0f);
}

}  // namespace

void ResetKanji() {
    kind_by_body_id.clear();
}

void BuildKanji(BODY* body, Kanji kind) {
    AddParts(body, GetTables().forms[ToIndex(kind)]);
    body->CalculateMass();
}

BODY* PutKanji(Kanji kind, VEC2 center, float angle) {
    BODY* body = AddBody(center, angle, GetKanjiColor(kind));
    if (body == nullptr) {
        return nullptr;
    }

    BuildKanji(body, kind);
    kind_by_body_id[body->GetID()] = kind;
    return body;
}

bool MergeKanji(Kanji* result) {
    const Tables& tables = GetTables();
    int previous_a = -1;
    int previous_b = -1;

    for (int i = 0; i < GetContactCnt(); ++i) {
        int a = 0;
        int b = 0;
        GetContactPair(i, &a, &b);

        // 同じ組の接触点は続けて並んでいるので、組ごとに1回だけ調べます。
        if (a == previous_a && b == previous_b) {
            continue;
        }
        previous_a = a;
        previous_b = b;

        const std::optional<Kanji> kind_a = KindOf(a);
        const std::optional<Kanji> kind_b = KindOf(b);
        if (!kind_a || !kind_b) {
            continue;
        }

        const int recipe = tables.recipe_index[ToIndex(*kind_a)][ToIndex(*kind_b)];
        if (recipe < 0) {
            continue;
        }

        *result = kRecipes[recipe].result;
        Fuse(a, b, *result);
        return true;
    }

    return false;
}

bool GetBodyKanji(int body_index, Kanji* kind) {
    const std::optional<Kanji> found = KindOf(body_index);
    if (!found) {
        return false;
    }

    *kind = *found;
    return true;
}

const wchar_t* GetKanjiName(Kanji kind) {
    return kInfos[ToIndex(kind)].name;
}

int GetKanjiStrokeCount(Kanji kind) {
    return kInfos[ToIndex(kind)].stroke_count;
}

UINT GetKanjiColor(Kanji kind) {
    return kInfos[ToIndex(kind)].color;
}

bool GetRecipeText(Kanji result, wchar_t* buffer, int buffer_length) {
    for (const Recipe& recipe : kRecipes) {
        if (recipe.result != result) {
            continue;
        }

        // 偏を使う字は、どの偏になるかを後ろに書き添えます(例: 「水+木 → 沐(氵)」)。
        const wchar_t* radical = nullptr;
        for (const Piece& piece : recipe.pieces) {
            if (piece.shape >= kNinben && piece.shape <= kSanzui) {
                radical = kRadicalNames[piece.shape - kNinben];
            }
        }

        if (radical != nullptr) {
            swprintf_s(buffer, buffer_length, L"%ls+%ls → %ls(%ls)",
                       GetKanjiName(recipe.a), GetKanjiName(recipe.b), GetKanjiName(recipe.result), radical);
        } else {
            swprintf_s(buffer, buffer_length, L"%ls+%ls → %ls",
                       GetKanjiName(recipe.a), GetKanjiName(recipe.b), GetKanjiName(recipe.result));
        }
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// 字の形の編集
// ---------------------------------------------------------------------------

const wchar_t* GetEditShapeName(int shape) {
    if (shape < kBaseKanjiCount) {
        return GetKanjiName(kBaseKanji[shape]);
    }
    return kRadicalNames[shape - kBaseKanjiCount];
}

bool GetEditShapeKanji(int shape, Kanji* kind) {
    if (shape >= kBaseKanjiCount) {
        return false;
    }
    *kind = kBaseKanji[shape];
    return true;
}

int GetEditShapeStrokes(int shape, Stroke* out) {
    const StrokeList& strokes = GetShapeData().shapes[EditShapeToShapeId(shape)];
    const int count = static_cast<int>(strokes.size());
    for (int i = 0; i < count; ++i) {
        out[i] = strokes[i];
    }
    return count;
}

bool SetEditShapeStrokes(int shape, const Stroke* strokes, int count) {
    if (count <= 0 || count > kMaxShapeStrokes) {
        return false;
    }
    for (int i = 0; i < count; ++i) {
        if ((strokes[i].end - strokes[i].start).Length() < kMinStrokeLength) {
            return false;
        }
    }

    // 作り直せたときだけ入れ替えます。
    ShapeData& data = GetShapeData();
    std::array<StrokeList, kShapeCount> shapes = data.shapes;
    shapes[EditShapeToShapeId(shape)].assign(strokes, strokes + count);

    Tables tables;
    if (!BuildTables(shapes, &tables)) {
        return false;
    }

    data.shapes = shapes;
    data.tables = tables;
    return true;
}

void MakeStrokePoints(const Stroke& stroke, VEC2* points) {
    const Quad quad = MakeStrokeQuad(stroke);
    for (int i = 0; i < 4; ++i) {
        points[i] = quad[i];
    }
}

bool SaveKanjiData() {
    std::ofstream file(kKanjiDataFile, std::ios::binary);
    if (!file) {
        return false;
    }

    file << "# 漢字タワーバトルの字の形(エディタで保存)\n";
    file << "# 1行に画1本: 形 始点x 始点y 終点x 終点y 始点の端 終点の端\n";
    file << "# 座標はマス(1字は10×10マス。左上が原点で、下が正)。端は keep(そのまま) / extend(伸ばす) / trim(縮める) / flat(平らな足)\n";

    const ShapeData& data = GetShapeData();
    char line[128];

    for (int i = 0; i < kEditShapeCount; ++i) {
        const std::string name = ToUtf8(GetEditShapeName(i));
        for (const Stroke& stroke : data.shapes[EditShapeToShapeId(i)]) {
            std::snprintf(line, sizeof(line), " %g %g %g %g %s %s\n",
                          stroke.start.x, stroke.start.y, stroke.end.x, stroke.end.y,
                          EndToText(stroke.start_end), EndToText(stroke.end_end));
            file << name << line;
        }
    }

    return static_cast<bool>(file);
}
