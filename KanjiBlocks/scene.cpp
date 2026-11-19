/**
 * @file scene.cpp
 * @brief 画面の切り替えの実装です。
 *
 * このファイルは AI(Claude)が書いたものです。
 */
#include "scene.h"

#include <optional>

namespace {

/** 次のフレームの先頭で行う切り替えです。 */
struct PendingChange {
    SceneId id;
    bool is_back;  ///< 1つ前の画面に戻る切り替えなら true です。Enter を呼びません。
};

SceneId current_id = SceneId::kTitle;
SceneId previous_id = SceneId::kTitle;
Scene* current = nullptr;
std::optional<PendingChange> pending_change;
bool is_quit_requested = false;

Scene* GetScene(SceneId id) {
    switch (id) {
        case SceneId::kTitle:
            return GetTitleScene();
        case SceneId::kHowto:
            return GetHowtoScene();
        case SceneId::kPlay:
            return GetPlayScene();
        case SceneId::kResult:
            return GetResultScene();
        case SceneId::kCollection:
            return GetCollectionScene();
        case SceneId::kEditor:
            return GetEditorScene();
    }
    return GetTitleScene();
}

/** 待っている切り替えがあれば行います。 */
void ApplyPendingChange() {
    if (!pending_change) {
        return;
    }

    const PendingChange change = *pending_change;
    pending_change.reset();

    previous_id = current_id;
    current_id = change.id;
    current = GetScene(current_id);
    if (!change.is_back) {
        current->Enter();
    }
}

}  // namespace

void StartScene(SceneId id) {
    current_id = id;
    previous_id = id;
    current = GetScene(id);
    current->Enter();
}

void ChangeScene(SceneId id) {
    pending_change = PendingChange{id, false};
}

void BackScene() {
    pending_change = PendingChange{previous_id, true};
}

void QuitGame() {
    is_quit_requested = true;
}

bool IsQuitRequested() {
    return is_quit_requested;
}

void UpdateScene() {
    ApplyPendingChange();
    current->Update();
}

void RenderScene(HDC hdc) {
    current->Render(hdc);
}

void SceneKeyDown(int key) {
    current->OnKeyDown(key);
}

void SceneMouseDown(int x, int y) {
    current->OnMouseDown(x, y);
}

void SceneMouseMove(int x, int y) {
    current->OnMouseMove(x, y);
}

void SceneMouseUp(int x, int y) {
    current->OnMouseUp(x, y);
}
