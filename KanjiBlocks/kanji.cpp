/**
 * @file kanji.cpp
 * @brief 字を物理に入れる処理と、合体の実装です。
 *
 * 形のデータは kanji_shape.cpp、データファイルは kanji_file.cpp にあります。
 *
 * 合体について
 *   - 合体の表にある組み合わせの2字が触れたら、向きや並び方に関係なく1字に置き換えます。
 *   - 合体してできた字は、2字をひとまとまりの物体とみなしたときの速度と角速度を引き継ぎます。
 *
 * このファイルは AI(Claude)が書いたものです。
 */
#include "kanji.h"

#include <cmath>
#include <optional>
#include <unordered_map>

#include "kanji_shape.h"

using namespace kanji_detail;

namespace {

/** 物理に入れた字の記録です。物体の通し番号から字の種類を引きます。 */
std::unordered_map<int, Kanji> kind_by_body_id;

std::optional<Kanji> KindOf(int body_index) {
    const auto found = kind_by_body_id.find(bodies[body_index].GetID());
    if (found == kind_by_body_id.end()) {
        return std::nullopt;
    }
    return found->second;
}

/**
 * 物体 index_a と index_b を、合体してできる字1つに置き換えます。
 *
 * 位置は2字の重心(質量で重みづけ)、向きは2字の向きを質量で重みづけした平均にします。
 * 速度は、2字をひとまとまりの物体とみなしたときの重心の速度にします。
 *
 * 角速度は 0 にします。
 * 2字の角運動量を引き継ぐと、まっすぐ落として合体させても、少しずれて当たっただけで回り出して不自然に見えるためです。
 *
 * @return できた字の物体です。
 */
BODY* Fuse(int index_a, int index_b, Kanji result) {
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
        return nullptr;
    }

    body->SetVelocity(velocity);
    body->SetAngularVelocity(0.0f);
    return body;
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

bool CanMergeKanji(Kanji a, Kanji b) {
    return GetTables().recipe_index[ToIndex(a)][ToIndex(b)] >= 0;
}

bool GetBodyKanji(int body_index, Kanji* kind) {
    const std::optional<Kanji> found = KindOf(body_index);
    if (!found) {
        return false;
    }

    *kind = *found;
    return true;
}
