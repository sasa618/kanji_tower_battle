/**
 * @file scene_play.cpp
 * @brief プレイ画面とリザルト画面のクラスです。
 *
 * キーの操作をゲームのルール(game.cpp)に伝えて、ゲームを進めます。
 * 表示とボタンは view_play.cpp にあります。
 *
 * このファイルは AI(Claude)が書いたものです。
 */
#include "game.h"
#include "scene.h"
#include "view.h"

namespace {

/** プレイ画面です。 */
class PlayScene : public Scene {
public:
    void Enter() override {
        ResetGame();
        is_paused_ = false;
    }

    void Update() override {
        if (is_paused_) {
            return;
        }

        UpdateGame();

        // ゲームオーバーになったら、すぐにリザルト画面に移ります。
        if (GetGame()->isGameOver) {
            ChangeScene(SceneId::kResult);
        }
    }

    void Render(HDC hdc) override { RenderPlayView(hdc, is_paused_); }

    void OnKeyDown(int key) override {
        switch (key) {
            case VK_SPACE:
                is_paused_ = !is_paused_;
                break;
            case 'R':
                Enter();
                break;
            case VK_ESCAPE:
                ChangeScene(SceneId::kTitle);
                break;
            case 'B':
                ChangeScene(SceneId::kCollection);
                break;

            // ここから下は落とす前の字の操作で、落とした後は効きません。
            case VK_LEFT:
                MoveCurKanji(-1);
                break;
            case VK_RIGHT:
                MoveCurKanji(1);
                break;
            case VK_UP:
                RotateCurKanji();
                break;
            case VK_DOWN:
                DropCurKanji();
                break;
            case '1':
            case '2':
            case '3':
                SelectHand(key - '1');
                break;
            default:
                break;
        }
    }

    void OnMouseMove(int x, int y) override { PlayViewMouseMove(x, y); }

    void OnMouseDown(int x, int y) override {
        // 漢字図鑑を開いている間はゲームが止まり、図鑑を閉じると続きに戻ります。
        if (PlayViewMouseDown(x, y)) {
            ChangeScene(SceneId::kCollection);
        }
    }

private:
    bool is_paused_ = false;  ///< 一時停止していれば true です。
};

/**
 * リザルト画面です。
 * ゲームオーバーになった時点で、プレイ画面の上に結果を重ねて出します。
 */
class ResultScene : public Scene {
public:
    void Enter() override { OpenResultView(); }

    /** 後ろで字が落ちていく様子が見えるように、物理演算は進めます。 */
    void Update() override { UpdateGame(); }

    void Render(HDC hdc) override { RenderResultView(hdc); }

    void OnKeyDown(int key) override { Decide(ResultViewKeyDown(key)); }
    void OnMouseMove(int x, int y) override { ResultViewMouseMove(x, y); }
    void OnMouseDown(int x, int y) override { Decide(ResultViewMouseDown(x, y)); }

private:
    /** 決めたボタンに応じて、画面を切り替えます。 */
    void Decide(ResultButton button) {
        switch (button) {
            case ResultButton::kRetry:
                ChangeScene(SceneId::kPlay);
                break;
            case ResultButton::kTitle:
                ChangeScene(SceneId::kTitle);
                break;
            case ResultButton::kNone:
                break;
        }
    }
};

}  // namespace

Scene* GetPlayScene() {
    static PlayScene scene;
    return &scene;
}

Scene* GetResultScene() {
    static ResultScene scene;
    return &scene;
}
