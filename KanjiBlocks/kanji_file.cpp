/**
 * @file kanji_file.cpp
 * @brief 字の形のデータファイルの読み書きの実装です。
 *
 * このファイルは AI(Claude)が書いたものです。
 */
#include <cstdio>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

#include "kanji_shape.h"

using namespace kanji_detail;

namespace {

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

/** 名前(UTF-8)から、編集できる形の番号を探します。 */
std::optional<int> FindEditShape(const std::string& name) {
    for (int i = 0; i < kEditShapeCount; ++i) {
        if (ToUtf8(GetEditShapeName(i)) == name) {
            return i;
        }
    }
    return std::nullopt;
}

}  // namespace

std::string ToUtf8(const wchar_t* text) {
    char buffer[16] = {};
    WideCharToMultiByte(CP_UTF8, 0, text, -1, buffer, sizeof(buffer), nullptr, nullptr);
    return buffer;
}

namespace kanji_detail {

void LoadDataFile(ShapeList* shapes) {
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
}  // namespace kanji_detail

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
