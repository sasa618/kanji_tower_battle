/**
 * @file scene.h
 * @brief 画面の基底クラスと、画面の切り替えです。
 *
 * 画面ごとに Scene を継承したクラスを作り、使う処理だけを書き換えます。
 * メインループは、今の画面が何かを知らずに UpdateScene や RenderScene を呼びます。
 *
 * このファイルは AI(Claude)が書いたものです。
 */
#pragma once

#include <windows.h>

/** 画面の種類です。 */
enum class SceneId {
    kTitle,       ///< タイトル画面
    kHowto,       ///< 遊び方画面
    kPlay,        ///< プレイ画面
    kResult,      ///< リザルト画面
    kCollection,  ///< 漢字図鑑画面
    kEditor,      ///< エディタ画面
};

/**
 * 画面の基底クラスです。
 * 描画のほかは何もしない処理を既定にしてあるので、派生クラスでは使う処理だけを書き換えます。
 */
class Scene {
public:
    virtual ~Scene() = default;

    /** 画面に入ったときに呼ばれます。 */
    virtual void Enter() {}

    /** 1フレーム進めます。 */
    virtual void Update() {}

    virtual void Render(HDC hdc) = 0;

    virtual void OnKeyDown(int key) {}
    virtual void OnMouseDown(int x, int y) {}
    virtual void OnMouseMove(int x, int y) {}
    virtual void OnMouseUp(int x, int y) {}
};

// 画面ごとの実体です。それぞれの scene_*.cpp にあります。
Scene* GetTitleScene();
Scene* GetHowtoScene();
Scene* GetPlayScene();
Scene* GetResultScene();
Scene* GetCollectionScene();
Scene* GetEditorScene();

/** 最初の画面を開きます。 */
void StartScene(SceneId id);

/**
 * 画面を切り替えます。
 * 処理の途中で画面が変わらないように、次のフレームの先頭で切り替えて Enter を呼びます。
 */
void ChangeScene(SceneId id);

/** 1つ前の画面に戻ります。Enter は呼ばないので、戻った画面は元の続きから進みます。 */
void BackScene();

/** ゲームの終了を知らせます。ウィンドウは、メインループが IsQuitRequested を見て閉じます。 */
void QuitGame();

bool IsQuitRequested();

// 今の画面の処理を呼びます。
void UpdateScene();
void RenderScene(HDC hdc);
void SceneKeyDown(int key);
void SceneMouseDown(int x, int y);
void SceneMouseMove(int x, int y);
void SceneMouseUp(int x, int y);
