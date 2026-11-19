/**
 * @file scene_title.cpp
 * @brief タイトル画面と遊び方画面のクラスです。
 *
 * 表示とボタンは view_title.cpp にあり、ここでは押されたボタンに応じて画面を切り替えます。
 *
 * このファイルは AI(Claude)が書いたものです。
 */
#include "scene.h"
#include "view.h"

namespace {

/** タイトル画面です。 */
class TitleScene : public Scene {
public:
    void Enter() override { OpenTitleView(); }
    void Update() override { UpdateTitleView(); }
    void Render(HDC hdc) override { RenderTitleView(hdc); }

    void OnKeyDown(int key) override {
        // 字のエディタは、ボタンを出さずに E キーで開きます。
        if (key == 'E') {
            ChangeScene(SceneId::kEditor);
            return;
        }
        Decide(TitleViewKeyDown(key));
    }

    void OnMouseMove(int x, int y) override { TitleViewMouseMove(x, y); }
    void OnMouseDown(int x, int y) override { Decide(TitleViewMouseDown(x, y)); }

private:
    /** 決めたボタンに応じて、画面を切り替えるかゲームを終了します。 */
    void Decide(TitleButton button) {
        switch (button) {
            case TitleButton::kStart:
                ChangeScene(SceneId::kPlay);
                break;
            case TitleButton::kHowto:
                ChangeScene(SceneId::kHowto);
                break;
            case TitleButton::kQuit:
                QuitGame();
                break;
            case TitleButton::kNone:
                break;
        }
    }
};

/** 遊び方画面です。戻るボタンか Esc で、タイトル画面に戻ります。 */
class HowtoScene : public Scene {
public:
    void Enter() override { OpenHowtoView(); }
    void Render(HDC hdc) override { RenderHowtoView(hdc); }

    void OnKeyDown(int key) override {
        if (key == VK_ESCAPE || HowtoViewKeyDown(key)) {
            ChangeScene(SceneId::kTitle);
        }
    }

    void OnMouseDown(int x, int y) override {
        if (HowtoViewMouseDown(x, y)) {
            ChangeScene(SceneId::kTitle);
        }
    }
};

}  // namespace

Scene* GetTitleScene() {
    static TitleScene scene;
    return &scene;
}

Scene* GetHowtoScene() {
    static HowtoScene scene;
    return &scene;
}
