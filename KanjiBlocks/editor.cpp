/**
 * @file editor.cpp
 * @brief 字のエディタの実装です。
 *
 *   - 左のキャンバス(10×10 マス)の上でドラッグして画を引き、画の端の点をドラッグして端を動かします。
 *   - 形を変えるたびに SetEditShapeStrokes を呼び、合体してできる字も含めてゲームの字の形を作り直します。
 *     作り直せない形(画が無い・短すぎる・部品が多すぎる)は受け付けず、元の形のままにします。
 *   - 右上の枠で、編集している字を試しに落とせます。このとき物体0番がステージ、1番が落とした字です。
 *   - 下には、合体してできる字も含めたすべての字を、今の形で並べます。
 *
 * このファイルは AI(Claude)が書いたものです。
 */
#include "editor.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

#include "config.h"
#include "draw.h"
#include "kanji.h"
#include "kanji_draw.h"
#include "physics.h"

namespace {

// キャンバス(画を引くマス目)の位置と大きさです。
constexpr int kCanvasLeft = 40;
constexpr int kCanvasTop = 60;
constexpr float kCellSize = 40.0f;
constexpr int kCanvasSize = static_cast<int>(kCellSize * kKanjiGrid);

/** マウスの位置をそろえる刻み(マス)です。 */
constexpr float kSnapStep = 0.1f;

/** 画の端をつかめる距離(マス)です。 */
constexpr float kGrabDistance = 0.35f;

/** 画を選べる距離(マス)です。中心線からこれより近ければ、その画を選びます。 */
constexpr float kPickDistance = 0.8f;

// 試しに落とす枠と、その中のステージです。
constexpr int kTestLeft = 480;
constexpr int kTestRight = 940;
constexpr int kTestTop = 60;
constexpr int kTestBottom = 330;
constexpr float kTestStageLeft = 560.0f;
constexpr float kTestStageRight = 860.0f;
constexpr float kTestStageTop = 300.0f;
constexpr float kTestStageThickness = 12.0f;

/** 試しに落とす字の重心の位置です。 */
const VEC2 kTestDropCenter = VEC2(710.0f, 140.0f);

/** 傾けて落とすときの角度(ラジアン)です。約17度です。 */
constexpr float kTestTiltAngle = 0.3f;

/** 落とした字が止まったとき、傾きがこれより小さければ「立った」とみなします(度)。 */
constexpr float kStandingAngleDegrees = 5.0f;

// 操作の説明の欄です。
constexpr int kHelpLeft = 480;
constexpr int kHelpTop = 345;
constexpr int kHelpLineHeight = 22;

// 仕上がりの見本(すべての字)の並べ方です。2行に収まるように縮めて描きます。
constexpr int kSampleTop = 570;
constexpr int kSampleColumnSpacing = 76;
constexpr int kSampleRowSpacing = 85;
constexpr int kSampleColumnCount = 12;
constexpr float kSampleScale = 0.75f;

/** 知らせる文を出しておくフレーム数です。 */
constexpr int kMessageFrames = 120;

// 画面の色です。背景・ステージ・文字の色は config.h のものを使います。
constexpr UINT kGridColor = RGB(225, 225, 230);
constexpr UINT kFrameColor = RGB(150, 150, 160);
constexpr UINT kBaseLineColor = RGB(230, 120, 120);
constexpr UINT kStrokeColor = RGB(60, 120, 80);
constexpr UINT kSelectedStrokeColor = RGB(240, 140, 30);
constexpr UINT kNewStrokeColor = RGB(120, 170, 230);
constexpr UINT kStartPointColor = RGB(40, 90, 220);
constexpr UINT kEndPointColor = RGB(220, 50, 50);
constexpr UINT kErrorColor = RGB(210, 40, 40);

/** 操作の説明です。上から順に1行ずつ出します。 */
constexpr const wchar_t* kHelpLines[] = {
    L"ドラッグ: 画を引く",
    L"端の点をドラッグ: 端を動かす",
    L"クリック: 画を選ぶ",
    L"Delete: 選んだ画を消す   Z: 元に戻す",
    L"Q / W: 始点 / 終点の端の作り方を変える",
    L"S: kanji_data.txt に保存",
    L"E / Esc: タイトルに戻る",
    L"赤い線(10マス目)に足をそろえると立ちやすい",
};

/** ドラッグしているものです。 */
enum class DragTarget {
    kNone,
    kNewStroke,   ///< 新しく引いている画
    kStartPoint,  ///< 画の始点
    kEndPoint,    ///< 画の終点
};

/** エディタの状態です。 */
struct EditorState {
    int shape = 0;                                    ///< 編集している形(GetEditShapeName の番号)
    std::vector<Stroke> strokes;                      ///< 編集している形の画
    int selected = -1;                                ///< 選んでいる画の番号。選んでいないときは -1 です。
    std::optional<std::vector<Stroke>> undo_strokes;  ///< 元に戻すときに使う、直前の変更の前の画です。

    DragTarget drag = DragTarget::kNone;
    int drag_stroke = -1;  ///< 端を動かしている画の番号
    VEC2 drag_from;        ///< ドラッグを始めた位置(マス)
    VEC2 drag_to;          ///< 今のマウスの位置(マス)

    std::wstring message;           ///< 知らせる文
    int message_frames_left = 0;    ///< 知らせる文を出しておく残りのフレーム数
    bool message_is_error = false;  ///< 知らせる文がエラーなら true です。

    bool has_test_body = false;  ///< 試しに落とした字があれば true です。
};

EditorState state;

void ShowMessage(const wchar_t* text, bool is_error) {
    state.message = text;
    state.message_frames_left = kMessageFrames;
    state.message_is_error = is_error;
}

// ---------------------------------------------------------------------------
// 座標の変換
// ---------------------------------------------------------------------------

/** 画面の位置を、キャンバスのマスの位置にします。kSnapStep の刻みにそろえ、マス目の中に収めます。 */
VEC2 ScreenToGrid(int x, int y) {
    const auto snap = [](float value) {
        return std::clamp(std::floor(value / kSnapStep + 0.5f) * kSnapStep, 0.0f, kKanjiGrid);
    };
    return VEC2(snap((x - kCanvasLeft) / kCellSize), snap((y - kCanvasTop) / kCellSize));
}

/** キャンバスのマスの位置を、画面の位置にします。 */
VEC2 GridToScreen(VEC2 p) {
    return VEC2(kCanvasLeft + p.x * kCellSize, kCanvasTop + p.y * kCellSize);
}

bool IsInCanvas(int x, int y) {
    return x >= kCanvasLeft && x <= kCanvasLeft + kCanvasSize && y >= kCanvasTop && y <= kCanvasTop + kCanvasSize;
}

// ---------------------------------------------------------------------------
// 画の編集
// ---------------------------------------------------------------------------

/** 編集する形の画を、今のゲームの形から読み込みます。 */
void LoadShape() {
    Stroke buffer[kMaxShapeStrokes];
    const int count = GetEditShapeStrokes(state.shape, buffer);
    state.strokes.assign(buffer, buffer + count);
    state.selected = -1;
    state.undo_strokes.reset();
    state.drag = DragTarget::kNone;
}

/**
 * 画を置き換え、ゲームの字の形を作り直します。
 * 作り直せないときは元のままにして、そのことを知らせます。
 * @return 置き換えられたら true です。
 */
bool ApplyStrokes(const std::vector<Stroke>& new_strokes) {
    if (!SetEditShapeStrokes(state.shape, new_strokes.data(), static_cast<int>(new_strokes.size()))) {
        ShowMessage(L"この形にはできません(画が無い・短すぎる・多すぎる)", true);
        return false;
    }

    state.undo_strokes = state.strokes;
    state.strokes = new_strokes;
    return true;
}

/** 点 p から線分 ab までの距離を返します。 */
float DistanceToSegment(VEC2 p, VEC2 a, VEC2 b) {
    const VEC2 ab = b - a;
    const float length_squared = Dot(ab, ab);
    if (length_squared == 0.0f) {
        return (p - a).Length();
    }

    const float t = std::clamp(Dot(p - a, ab) / length_squared, 0.0f, 1.0f);
    return (p - (a + ab * t)).Length();
}

/** 点 p にいちばん近い画の番号を返します。kPickDistance より遠ければ -1 です。 */
int FindStrokeAt(VEC2 p) {
    int nearest = -1;
    float nearest_distance = kPickDistance;

    for (int i = 0; i < static_cast<int>(state.strokes.size()); ++i) {
        const float distance = DistanceToSegment(p, state.strokes[i].start, state.strokes[i].end);
        if (distance < nearest_distance) {
            nearest_distance = distance;
            nearest = i;
        }
    }

    return nearest;
}

/** 点 p の近くで見つかった画の端です。 */
struct EndPointHit {
    int stroke;     ///< 画の番号
    bool is_start;  ///< 始点なら true、終点なら false です。
};

std::optional<EndPointHit> HitEndPoint(int stroke_index, VEC2 p) {
    const Stroke& stroke = state.strokes[stroke_index];
    if ((stroke.start - p).Length() < kGrabDistance) {
        return EndPointHit{stroke_index, true};
    }
    if ((stroke.end - p).Length() < kGrabDistance) {
        return EndPointHit{stroke_index, false};
    }
    return std::nullopt;
}

/** 点 p の近くにある画の端を探します。端が重なっているときに選んでいる画を動かせるよう、選んでいる画から先に調べます。 */
std::optional<EndPointHit> FindEndPointAt(VEC2 p) {
    if (state.selected >= 0) {
        if (const std::optional<EndPointHit> hit = HitEndPoint(state.selected, p)) {
            return hit;
        }
    }

    for (int i = 0; i < static_cast<int>(state.strokes.size()); ++i) {
        if (const std::optional<EndPointHit> hit = HitEndPoint(i, p)) {
            return hit;
        }
    }

    return std::nullopt;
}

/** 端の作り方を、そのまま → 伸ばす → 縮める → 平らな足 → そのまま の順に次のものにします。 */
StrokeEnd NextStrokeEnd(StrokeEnd type) {
    switch (type) {
        case StrokeEnd::kKeep:
            return StrokeEnd::kExtend;
        case StrokeEnd::kExtend:
            return StrokeEnd::kTrim;
        case StrokeEnd::kTrim:
            return StrokeEnd::kFlat;
        default:
            return StrokeEnd::kKeep;
    }
}

const wchar_t* StrokeEndLabel(StrokeEnd type) {
    switch (type) {
        case StrokeEnd::kKeep:
            return L"そのまま";
        case StrokeEnd::kExtend:
            return L"伸ばす";
        case StrokeEnd::kTrim:
            return L"縮める";
        case StrokeEnd::kFlat:
            return L"平らな足";
        default:
            return L"";
    }
}

/** 選んでいる画の、始点か終点の端の作り方を次のものにします。 */
void CycleSelectedEnd(bool start) {
    if (state.selected < 0) {
        ShowMessage(L"先に画をクリックして選んでください", true);
        return;
    }

    std::vector<Stroke> edited = state.strokes;
    Stroke& stroke = edited[state.selected];
    if (start) {
        stroke.start_end = NextStrokeEnd(stroke.start_end);
    } else {
        stroke.end_end = NextStrokeEnd(stroke.end_end);
    }
    ApplyStrokes(edited);
}

void DeleteSelectedStroke() {
    if (state.selected < 0) {
        return;
    }

    std::vector<Stroke> edited = state.strokes;
    edited.erase(edited.begin() + state.selected);
    if (ApplyStrokes(edited)) {
        state.selected = -1;
    }
}

/** 直前の変更の前の画に戻します。もう一度呼ぶと、戻す前の画に戻ります。 */
void Undo() {
    if (!state.undo_strokes) {
        return;
    }

    // ApplyStrokes が undo_strokes を書き換えるので、先に写しておきます。
    const std::vector<Stroke> restored = *state.undo_strokes;
    if (ApplyStrokes(restored)) {
        state.selected = -1;
    }
}

/** 編集する形を切り替えます。step が 1 なら次、-1 なら前の形にします。 */
void SwitchShape(int step) {
    state.shape = (state.shape + step + kEditShapeCount) % kEditShapeCount;
    LoadShape();
}

// ---------------------------------------------------------------------------
// 試しに落とす
// ---------------------------------------------------------------------------

/** 編集している字を、試しに落とす枠のステージの上から落とします。偏は字ではないので落とせません。 */
void DropTestKanji(float angle) {
    Kanji kind;
    if (!GetEditShapeKanji(state.shape, &kind)) {
        ShowMessage(L"偏は字ではないので落とせません(休・徊・沐で見てください)", true);
        return;
    }

    ClearBodies();
    ResetKanji();

    const VEC2 stage_center((kTestStageLeft + kTestStageRight) / 2.0f, kTestStageTop + kTestStageThickness / 2.0f);
    AddWall(stage_center, kTestStageRight - kTestStageLeft, kTestStageThickness, kStageColor);
    PutKanji(kind, kTestDropCenter, angle);

    state.has_test_body = true;
}

/** 試しに落とした字の傾きを、-180 度から 180 度の範囲で返します。 */
float TestKanjiAngleDegrees() {
    float degrees = std::fmod(bodies[1].GetAngle(), 2.0f * pi) * 180.0f / pi;
    if (degrees > 180.0f) {
        degrees -= 360.0f;
    }
    if (degrees < -180.0f) {
        degrees += 360.0f;
    }
    return degrees;
}

// ---------------------------------------------------------------------------
// 描画
// ---------------------------------------------------------------------------

void DrawHorizontalLine(HDC hdc, int left, int right, int y, UINT color) {
    FillBox(hdc, left, y, right, y + 1, color);
}

void DrawVerticalLine(HDC hdc, int x, int top, int bottom, UINT color) {
    FillBox(hdc, x, top, x + 1, bottom, color);
}

void DrawFrame(HDC hdc, int left, int top, int right, int bottom, UINT color) {
    DrawHorizontalLine(hdc, left, right, top, color);
    DrawHorizontalLine(hdc, left, right, bottom, color);
    DrawVerticalLine(hdc, left, top, bottom, color);
    DrawVerticalLine(hdc, right, top, bottom, color);
}

/** 画の端の点を、一辺 2 × half_size + 1 の四角で描きます。 */
void DrawEndPoint(HDC hdc, VEC2 grid_position, int half_size, UINT color) {
    const VEC2 screen = GridToScreen(grid_position);
    const int x = static_cast<int>(screen.x);
    const int y = static_cast<int>(screen.y);
    FillBox(hdc, x - half_size, y - half_size, x + half_size + 1, y + half_size + 1, color);
}

/** 画1本を、ゲームと同じ部品の形でキャンバスの上に描きます。 */
void DrawStroke(HDC hdc, const Stroke& stroke, UINT color) {
    VEC2 points[4];
    MakeStrokePoints(stroke, points);

    // MakeStrokePoints は1マス kKanjiUnit px で返すので、キャンバスのマスの大きさに広げます。
    constexpr float kScale = kCellSize / kKanjiUnit;
    for (VEC2& point : points) {
        point = VEC2(kCanvasLeft + point.x * kScale, kCanvasTop + point.y * kScale);
    }
    FillPolygon(hdc, points, 4, color, color);
}

void DrawCanvas(HDC hdc) {
    const int canvas_right = kCanvasLeft + kCanvasSize;
    const int canvas_bottom = kCanvasTop + kCanvasSize;

    for (int i = 0; i <= static_cast<int>(kKanjiGrid); ++i) {
        const int offset = static_cast<int>(i * kCellSize);
        DrawVerticalLine(hdc, kCanvasLeft + offset, kCanvasTop, canvas_bottom, kGridColor);
        DrawHorizontalLine(hdc, kCanvasLeft, canvas_right, kCanvasTop + offset, kGridColor);
    }

    // 10マス目は足をそろえる線なので、太く色を変えて描きます。
    FillBox(hdc, kCanvasLeft, canvas_bottom - 1, canvas_right + 1, canvas_bottom + 2, kBaseLineColor);

    // 端をドラッグしている画は、動かした後の形で描きます。
    for (int i = 0; i < static_cast<int>(state.strokes.size()); ++i) {
        Stroke stroke = state.strokes[i];
        if (i == state.drag_stroke) {
            if (state.drag == DragTarget::kStartPoint) {
                stroke.start = state.drag_to;
            } else if (state.drag == DragTarget::kEndPoint) {
                stroke.end = state.drag_to;
            }
        }
        DrawStroke(hdc, stroke, i == state.selected ? kSelectedStrokeColor : kStrokeColor);
    }

    if (state.drag == DragTarget::kNewStroke && (state.drag_to - state.drag_from).Length() >= kMinStrokeLength) {
        Stroke new_stroke;
        new_stroke.start = state.drag_from;
        new_stroke.end = state.drag_to;
        DrawStroke(hdc, new_stroke, kNewStrokeColor);
    }

    // 端の点は、始点を青、終点を赤で描きます。選んでいる画の点は大きくします。
    for (int i = 0; i < static_cast<int>(state.strokes.size()); ++i) {
        const int half_size = (i == state.selected) ? 5 : 3;
        DrawEndPoint(hdc, state.strokes[i].start, half_size, kStartPointColor);
        DrawEndPoint(hdc, state.strokes[i].end, half_size, kEndPointColor);
    }
}

/** 試しに落とす枠を描きます。落とした字があれば、その傾きと、止まったかどうかも出します。 */
void DrawTestArea(HDC hdc) {
    DrawFrame(hdc, kTestLeft, kTestTop, kTestRight, kTestBottom, kFrameColor);
    DrawString(hdc, kTestLeft + 8, kTestTop + 6, L"試しに落とす(T: まっすぐ  G: 傾けて)", kTextColor);

    if (!state.has_test_body) {
        FillBox(hdc, static_cast<int>(kTestStageLeft), static_cast<int>(kTestStageTop),
                static_cast<int>(kTestStageRight), static_cast<int>(kTestStageTop + kTestStageThickness), kStageColor);
        return;
    }

    for (int i = 0; i < bodyCnt; ++i) {
        DrawBody(hdc, &bodies[i], bodies[i].GetRGB(), 0.0f);
    }
    if (bodyCnt < 2) {
        return;
    }

    const float degrees = TestKanjiAngleDegrees();
    const wchar_t* status = L"動いている";
    if (bodies[1].IsResting()) {
        status = (std::fabs(degrees) < kStandingAngleDegrees) ? L"立った" : L"倒れた";
    }

    wchar_t text[64];
    swprintf_s(text, L"傾き: %.0f度  %ls", degrees, status);
    DrawString(hdc, kTestLeft + 8, kTestBottom - 26, text, kTextColor);
}

/** 合体してできる字も含めたすべての字を、今の形で並べて描きます。 */
void DrawSamples(HDC hdc) {
    DrawString(hdc, kCanvasLeft, kSampleTop - 24, L"仕上がり(合体してできる字も含む)", kTextColor);

    for (int i = 0; i < kKanjiKindCount; ++i) {
        const float center_x = kCanvasLeft + 40.0f + (i % kSampleColumnCount) * kSampleColumnSpacing;
        const float center_y = kSampleTop + 45.0f + (i / kSampleColumnCount) * kSampleRowSpacing;
        DrawKanji(hdc, static_cast<Kanji>(i), VEC2(center_x, center_y), kSampleScale);
    }
}

}  // namespace

void OpenEditor() {
    ClearBodies();
    ResetKanji();
    state.has_test_body = false;
    state.message_frames_left = 0;

    LoadShape();
}

void UpdateEditor() {
    if (state.has_test_body) {
        StepPhysics(kFrameTime);
    }
    if (state.message_frames_left > 0) {
        --state.message_frames_left;
    }
}

void EditorKeyDown(int key) {
    switch (key) {
        case VK_LEFT:
            SwitchShape(-1);
            break;
        case VK_RIGHT:
            SwitchShape(1);
            break;
        case VK_DELETE:
        case VK_BACK:
            DeleteSelectedStroke();
            break;
        case 'Q':
            CycleSelectedEnd(true);
            break;
        case 'W':
            CycleSelectedEnd(false);
            break;
        case 'Z':
            Undo();
            break;
        case 'T':
            DropTestKanji(0.0f);
            break;
        case 'G':
            DropTestKanji(kTestTiltAngle);
            break;
        case 'S':
            if (SaveKanjiData()) {
                ShowMessage(L"kanji_data.txt に保存しました", false);
            } else {
                ShowMessage(L"保存できませんでした", true);
            }
            break;
        default:
            break;
    }
}

void EditorMouseDown(int x, int y) {
    if (!IsInCanvas(x, y)) {
        return;
    }

    const VEC2 p = ScreenToGrid(x, y);
    state.drag_from = p;
    state.drag_to = p;

    if (const std::optional<EndPointHit> hit = FindEndPointAt(p)) {
        state.drag = hit->is_start ? DragTarget::kStartPoint : DragTarget::kEndPoint;
        state.drag_stroke = hit->stroke;
        state.selected = hit->stroke;
        return;
    }

    state.drag = DragTarget::kNewStroke;
}

void EditorMouseMove(int x, int y) {
    if (state.drag == DragTarget::kNone) {
        return;
    }

    state.drag_to = ScreenToGrid(x, y);
}

void EditorMouseUp(int x, int y) {
    if (state.drag == DragTarget::kNone) {
        return;
    }

    state.drag_to = ScreenToGrid(x, y);
    const DragTarget finished = state.drag;
    state.drag = DragTarget::kNone;

    std::vector<Stroke> edited = state.strokes;

    if (finished == DragTarget::kNewStroke) {
        // ほとんど動かしていなければ、クリックとみなして画を選びます。
        if ((state.drag_to - state.drag_from).Length() < kMinStrokeLength) {
            state.selected = FindStrokeAt(state.drag_from);
            return;
        }
        if (static_cast<int>(edited.size()) >= kMaxShapeStrokes) {
            ShowMessage(L"画が多すぎます", true);
            return;
        }

        Stroke new_stroke;
        new_stroke.start = state.drag_from;
        new_stroke.end = state.drag_to;
        edited.push_back(new_stroke);
        if (ApplyStrokes(edited)) {
            state.selected = static_cast<int>(state.strokes.size()) - 1;
        }
        return;
    }

    Stroke& moved = edited[state.drag_stroke];
    if (finished == DragTarget::kStartPoint) {
        moved.start = state.drag_to;
    } else {
        moved.end = state.drag_to;
    }
    ApplyStrokes(edited);
}

void RenderEditor(HDC hdc) {
    FillBox(hdc, 0, 0, kScreenWidth, kScreenHeight, kBackgroundColor);

    wchar_t text[96];
    swprintf_s(text, L"字のエディタ: %ls  (%d / %d、←→で切り替え)",
               GetEditShapeName(state.shape), state.shape + 1, kEditShapeCount);
    DrawString(hdc, kCanvasLeft, 24, text, kTextColor);

    DrawCanvas(hdc);

    const int status_top = kCanvasTop + kCanvasSize + 10;
    if (state.selected >= 0) {
        const Stroke& stroke = state.strokes[state.selected];
        swprintf_s(text, L"選んだ画: 始点 %ls (Q) / 終点 %ls (W)",
                   StrokeEndLabel(stroke.start_end), StrokeEndLabel(stroke.end_end));
    } else {
        swprintf_s(text, L"画の数: %d  (画をクリックすると選べます)", static_cast<int>(state.strokes.size()));
    }
    DrawString(hdc, kCanvasLeft, status_top, text, kTextColor);

    if (state.message_frames_left > 0) {
        DrawString(hdc, kCanvasLeft, status_top + 26, state.message.c_str(),
                   state.message_is_error ? kErrorColor : kTextColor);
    }

    DrawTestArea(hdc);

    int help_y = kHelpTop;
    for (const wchar_t* line : kHelpLines) {
        DrawString(hdc, kHelpLeft, help_y, line, kTextColor);
        help_y += kHelpLineHeight;
    }

    DrawSamples(hdc);
}
