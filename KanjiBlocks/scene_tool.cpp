/**
 * @file scene_tool.cpp
 * @brief 漢字図鑑画面とエディタ画面のクラスです。
 *
 * 処理の中身は collection.cpp と editor.cpp にあり、ここではそれを呼びます。
 *
 * このファイルは AI(Claude)が書いたものです。
 */
#include "collection.h"
#include "config.h"
#include "editor.h"
#include "scene.h"

namespace {

/** 漢字図鑑画面です。B か Esc で、図鑑を開く前の画面に戻ります。 */
class CollectionScene : public Scene {
public:
    void Render(HDC hdc) override { RenderCollection(hdc, kScreenWidth, kScreenHeight); }

    void OnKeyDown(int key) override {
        if (key == 'B' || key == VK_ESCAPE) {
            BackScene();
            return;
        }
        CollectionKeyDown(key);
    }

    void OnMouseDown(int x, int y) override { CollectionMouseDown(x, y); }
};

/** エディタ画面です。E か Esc で、タイトル画面に戻ります。 */
class EditorScene : public Scene {
public:
    void Enter() override { OpenEditor(); }
    void Update() override { UpdateEditor(); }
    void Render(HDC hdc) override { RenderEditor(hdc); }

    void OnKeyDown(int key) override {
        if (key == 'E' || key == VK_ESCAPE) {
            ChangeScene(SceneId::kTitle);
            return;
        }
        EditorKeyDown(key);
    }

    void OnMouseDown(int x, int y) override { EditorMouseDown(x, y); }
    void OnMouseMove(int x, int y) override { EditorMouseMove(x, y); }
    void OnMouseUp(int x, int y) override { EditorMouseUp(x, y); }
};

}  // namespace

Scene* GetCollectionScene() {
    static CollectionScene scene;
    return &scene;
}

Scene* GetEditorScene() {
    static EditorScene scene;
    return &scene;
}
