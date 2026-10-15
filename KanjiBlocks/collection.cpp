/**
 * @file collection.cpp
 * @brief 漢字図鑑(完成させた字のリスト)の実装です。
 *
 *   - 漢字No.は Kanji の順に 1 から付けます。基本の字が先で、合体してできる字が後です。
 *   - 字ごとに「落とした回数」と「合体して作った回数」を持ちます。
 *     どちらかが1回以上なら見つけた字とみなし、図鑑に形を出します。
 *   - 記録は1字1行のテキスト(No. 字 落とした回数 作った回数)で保存し、記録が変わるたびに書き直します。
 *   - 画面は、上に字の一覧、下に選んだ字の説明(読み・意味・熟語・作り方・作った回数)を出します。
 *     落とした回数は、見つけたかどうかを決めるためだけに使い、画面には出しません。
 *     まだ見つけていない字は、形も説明も出しません。
 *
 * このファイルは AI(Claude)が書いたものです。
 */
#include "collection.h"

#include <array>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "draw.h"

namespace {

/** 1字ぶんの記録です。 */
struct Entry {
    int drop_count = 0;  ///< 落とした回数
    int made_count = 0;  ///< 合体して作った回数

    bool IsFound() const {
        return drop_count > 0 || made_count > 0;
    }
};

using Entries = std::array<Entry, kKanjiKindCount>;

// 画面の並べ方です。上に字の一覧、下に選んだ字の説明を出します。
constexpr int kColumnCount = 6;
constexpr int kRowCount = (kKanjiKindCount + kColumnCount - 1) / kColumnCount;
constexpr int kCellWidth = 150;
constexpr int kCellHeight = 118;
constexpr int kGridLeft = 30;
constexpr int kGridTop = 56;
constexpr float kCellKanjiScale = 0.8f;
constexpr int kDetailTop = kGridTop + kRowCount * kCellHeight + 4;
constexpr int kDetailHeight = 216;
constexpr int kDetailTextLeft = kGridLeft + 170;
constexpr float kDetailKanjiScale = 1.4f;
constexpr int kLineHeight = 30;

// 画面の色です。
constexpr UINT kBackgroundColor = RGB(255, 255, 255);
constexpr UINT kFrameColor = RGB(200, 200, 210);
constexpr UINT kUnknownColor = RGB(235, 235, 240);
constexpr UINT kTextColor = RGB(40, 40, 40);
constexpr UINT kDimTextColor = RGB(150, 150, 160);
constexpr UINT kSelectColor = RGB(255, 240, 180);

/**
 * 字の読み・意味・熟語です。どれも主なものだけを載せます。
 * 読みは、カタカナが音読み、ひらがなが訓読みです。
 */
struct Description {
    const wchar_t* reading;
    const wchar_t* meaning;
    const wchar_t* words;
};

/** Kanji の順に並べた説明です。 */
constexpr Description kDescriptions[] = {
    {L"ボク・モク / き・こ", L"立っている木。材木", L"大木(たいぼく)、樹木(じゅもく)、木材(もくざい)"},
    {L"ニチ・ジツ / ひ・か", L"太陽。一日", L"毎日(まいにち)、日光(にっこう)、休日(きゅうじつ)"},
    {L"ゲツ・ガツ / つき", L"天体の月。こよみの月", L"満月(まんげつ)、正月(しょうがつ)、月光(げっこう)"},
    {L"ジン・ニン / ひと", L"ひと", L"人間(にんげん)、名人(めいじん)、人口(じんこう)"},
    {L"コウ・ク / くち", L"くち。出入りする所", L"出口(でぐち)、人口(じんこう)、口調(くちょう)"},
    {L"スイ / みず", L"みず", L"水道(すいどう)、海水(かいすい)、水分(すいぶん)"},
    {L"カ / ひ・ほ", L"ひ。もえること", L"火山(かざん)、花火(はなび)、火災(かさい)"},
    {L"リン / はやし", L"木がたくさん生えている所", L"森林(しんりん)、山林(さんりん)、林業(りんぎょう)"},
    {L"シン / もり", L"木がたくさん茂っている所", L"森林(しんりん)、森閑(しんかん)"},
    {L"メイ・ミョウ / あかり・あかるい・あきらか・あける", L"あかるい。はっきりしている", L"説明(せつめい)、文明(ぶんめい)、明日(みょうにち)"},
    {L"ホウ / とも", L"友だち。仲間", L"朋友(ほうゆう)、朋輩(ほうばい)"},
    {L"ショウ", L"さかんである", L"繁昌(はんじょう)、隆昌(りゅうしょう)"},
    {L"キュウ / やすむ・やすまる・やすめる", L"やすむ", L"休日(きゅうじつ)、休息(きゅうそく)、休憩(きゅうけい)"},
    {L"カイ・エ / まわる・まわす", L"まわる。めぐる。回数", L"回転(かいてん)、今回(こんかい)、回答(かいとう)"},
    {L"コン / こまる", L"こまる。くるしむ", L"困難(こんなん)、貧困(ひんこん)、困惑(こんわく)"},
    {L"トウ / くつ", L"くつ。重なり合う", L"雑沓(ざっとう)"},
    {L"エン / ほのお", L"ほのお。もえあがる", L"火炎(かえん)、炎上(えんじょう)、肺炎(はいえん)"},
    {L"カイ", L"さまよう", L"徘徊(はいかい)"},
    {L"モク", L"髪を洗う", L"沐浴(もくよく)"},
    {L"ショウ / となえる", L"となえる。声を出してうたう", L"合唱(がっしょう)、提唱(ていしょう)、唱歌(しょうか)"},
    {L"トウ / ひがし", L"ひがし", L"東京(とうきょう)、東西(とうざい)、関東(かんとう)"},
    {L"ショウ", L"きらきらと光る。鉱物の結晶", L"結晶(けっしょう)、水晶(すいしょう)、液晶(えきしょう)"},
    {L"フン / たく", L"火をつけて燃やす", L"焚書(ふんしょ)、焚き火(たきび)"},
    {L"シュウ", L"とらえられた人。とらえる", L"囚人(しゅうじん)、死刑囚(しけいしゅう)"},
};
static_assert(sizeof(kDescriptions) / sizeof(kDescriptions[0]) == kKanjiKindCount, "字ごとに説明が必要です。");

int ToIndex(Kanji kind) {
    return static_cast<int>(kind);
}

/**
 * 記録をファイルから読みます。ファイルが無いときは、すべて 0 回です。
 * ファイルは1行に1字で、「No. 字 落とした回数 作った回数」と書きます。
 */
Entries LoadEntries() {
    Entries entries;
    std::ifstream file(kCollectionFile, std::ios::binary);
    if (!file) {
        return entries;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::istringstream in(line);
        int number = 0;
        std::string name;
        Entry entry;
        if (!(in >> number >> name >> entry.drop_count >> entry.made_count)) {
            continue;
        }
        if (number < 1 || number > kKanjiKindCount) {
            continue;
        }

        entries[number - 1] = entry;
    }

    return entries;
}

Entries& GetEntries() {
    static Entries entries = LoadEntries();
    return entries;
}

std::string ToUtf8(const wchar_t* text) {
    char buffer[16] = {};
    WideCharToMultiByte(CP_UTF8, 0, text, -1, buffer, sizeof(buffer), nullptr, nullptr);
    return buffer;
}

void SaveEntries() {
    std::ofstream file(kCollectionFile, std::ios::binary);
    if (!file) {
        return;
    }

    file << "# 漢字図鑑の記録: No. 字 落とした回数 作った回数\n";

    const Entries& entries = GetEntries();
    for (int i = 0; i < kKanjiKindCount; ++i) {
        const Kanji kind = static_cast<Kanji>(i);
        file << GetKanjiNumber(kind) << ' ' << ToUtf8(GetKanjiName(kind)) << ' '
             << entries[i].drop_count << ' ' << entries[i].made_count << '\n';
    }
}

// ---------------------------------------------------------------------------
// 描画
// ---------------------------------------------------------------------------

void DrawFrame(HDC hdc, int left, int top, int right, int bottom, UINT color) {
    FillBox(hdc, left, top, right, top + 1, color);
    FillBox(hdc, left, bottom - 1, right, bottom, color);
    FillBox(hdc, left, top, left + 1, bottom, color);
    FillBox(hdc, right - 1, top, right, bottom, color);
}

/** 字の形を、重心が center に来るように scale 倍で描きます。 */
void DrawKanji(HDC hdc, Kanji kind, VEC2 center, float scale) {
    BODY sample;
    sample.Initialize(VEC2(), 0.0f, GetKanjiColor(kind));
    BuildKanji(&sample, kind);

    VEC2 points[maxPointCnt];
    for (int part = 0; part < sample.GetPartCnt(); ++part) {
        sample.GetWorldPoints(part, points);
        for (int i = 0; i < sample.GetPointCnt(part); ++i) {
            points[i] = center + points[i] * scale;
        }
        FillPolygon(hdc, points, sample.GetPointCnt(part), sample.GetRGB(), sample.GetRGB());
    }
}

/** 図鑑で選んでいる字の番号(Kanji の順)です。 */
int selected_index = 0;

/** 一覧の1字ぶんの欄を描きます。No.と、形(まだ見つけていなければ「？」)を出します。 */
void DrawCell(HDC hdc, int index, int left, int top) {
    const Kanji kind = static_cast<Kanji>(index);
    const int right = left + kCellWidth - 8;
    const int bottom = top + kCellHeight - 8;

    if (index == selected_index) {
        FillBox(hdc, left, top, right, bottom, kSelectColor);
    }
    DrawFrame(hdc, left, top, right, bottom, kFrameColor);

    wchar_t text[16];
    swprintf_s(text, L"No.%02d", GetKanjiNumber(kind));
    DrawString(hdc, left + 6, top + 4, text, kTextColor);

    const int center_x = (left + right) / 2;
    const int center_y = top + 62;
    if (GetEntries()[index].IsFound()) {
        DrawKanji(hdc, kind, VEC2(static_cast<float>(center_x), static_cast<float>(center_y)), kCellKanjiScale);
    } else {
        FillBox(hdc, center_x - 32, center_y - 32, center_x + 32, center_y + 32, kUnknownColor);
        DrawString(hdc, center_x - 6, center_y - 10, L"？", kDimTextColor);
    }
}

/** 選んでいる字の説明(読み・意味・熟語・作り方・作った回数)を、一覧の下に描きます。 */
void DrawDetail(HDC hdc, int width) {
    const int top = kDetailTop;
    DrawFrame(hdc, kGridLeft, top, width - kGridLeft, top + kDetailHeight, kFrameColor);

    const Kanji kind = static_cast<Kanji>(selected_index);
    const Entry& entry = GetEntries()[selected_index];
    wchar_t text[128];
    int y = top + 16;

    if (!entry.IsFound()) {
        swprintf_s(text, L"No.%02d  まだ見つけていない字です。落とすか、合体して作ると説明が出ます。", GetKanjiNumber(kind));
        DrawString(hdc, kGridLeft + 16, y, text, kDimTextColor);
        return;
    }

    DrawKanji(hdc, kind, VEC2(static_cast<float>(kGridLeft + 85), static_cast<float>(top + kDetailHeight / 2)),
              kDetailKanjiScale);

    const Description& description = kDescriptions[selected_index];
    swprintf_s(text, L"No.%02d  %ls  (%d画)", GetKanjiNumber(kind), GetKanjiName(kind), GetKanjiStrokeCount(kind));
    DrawString(hdc, kDetailTextLeft, y, text, kTextColor);
    y += kLineHeight;

    swprintf_s(text, L"読み: %ls", description.reading);
    DrawString(hdc, kDetailTextLeft, y, text, kTextColor);
    y += kLineHeight;

    swprintf_s(text, L"意味: %ls", description.meaning);
    DrawString(hdc, kDetailTextLeft, y, text, kTextColor);
    y += kLineHeight;

    swprintf_s(text, L"熟語: %ls", description.words);
    DrawString(hdc, kDetailTextLeft, y, text, kTextColor);
    y += kLineHeight;

    // 基本の字は合体では作れないので、作った回数は合体してできる字だけに出します。
    wchar_t recipe[64];
    if (!GetRecipeText(kind, recipe, 64)) {
        DrawString(hdc, kDetailTextLeft, y, L"作り方: 基本の字(そのまま落とせる字)", kTextColor);
        return;
    }

    swprintf_s(text, L"作り方: %ls", recipe);
    DrawString(hdc, kDetailTextLeft, y, text, kTextColor);
    y += kLineHeight;

    swprintf_s(text, L"作った回数: %d", entry.made_count);
    DrawString(hdc, kDetailTextLeft, y, text, kTextColor);
}

}  // namespace

int GetKanjiNumber(Kanji kind) {
    return ToIndex(kind) + 1;
}

void RecordDropped(Kanji kind) {
    ++GetEntries()[ToIndex(kind)].drop_count;
    SaveEntries();
}

void RecordMade(Kanji kind) {
    ++GetEntries()[ToIndex(kind)].made_count;
    SaveEntries();
}

int CountFoundKanji() {
    int count = 0;
    for (const Entry& entry : GetEntries()) {
        if (entry.IsFound()) {
            ++count;
        }
    }
    return count;
}

void RenderCollection(HDC hdc, int width, int height) {
    FillBox(hdc, 0, 0, width, height, kBackgroundColor);

    wchar_t text[128];
    swprintf_s(text, L"漢字図鑑  見つけた字: %d / %d   (←→↑↓ / クリック: 字を選ぶ   B / Esc: ゲームに戻る)",
               CountFoundKanji(), kKanjiKindCount);
    DrawString(hdc, kGridLeft, 20, text, kTextColor);

    for (int i = 0; i < kKanjiKindCount; ++i) {
        const int left = kGridLeft + (i % kColumnCount) * kCellWidth;
        const int top = kGridTop + (i / kColumnCount) * kCellHeight;
        DrawCell(hdc, i, left, top);
    }

    DrawDetail(hdc, width);
}

void CollectionKeyDown(int key) {
    int next = selected_index;
    switch (key) {
        case VK_LEFT:
            next = selected_index - 1;
            break;
        case VK_RIGHT:
            next = selected_index + 1;
            break;
        case VK_UP:
            next = selected_index - kColumnCount;
            break;
        case VK_DOWN:
            next = selected_index + kColumnCount;
            break;
        default:
            return;
    }

    if (next >= 0 && next < kKanjiKindCount) {
        selected_index = next;
    }
}

void CollectionMouseDown(int x, int y) {
    if (x < kGridLeft || y < kGridTop) {
        return;
    }

    const int column = (x - kGridLeft) / kCellWidth;
    const int row = (y - kGridTop) / kCellHeight;
    const int index = row * kColumnCount + column;
    if (column < kColumnCount && index < kKanjiKindCount) {
        selected_index = index;
    }
}
