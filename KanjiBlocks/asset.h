/**
 * @file asset.h
 * @brief data フォルダに置いた画像と動画を扱います。
 *
 * 画像は BMP を読んで描きます。動画は1コマずつ読んで描き、音は出しません。
 *
 * このファイルは AI(Claude)が書いたものです。
 */
#pragma once

#include <windows.h>

#include <optional>
#include <string>

struct IMFSourceReader;

/**
 * data フォルダの中のファイルの場所を探します。
 * 作業フォルダ、実行ファイルのフォルダ、その上のフォルダの順に探します。
 * @param name ファイルの名前(例: L"howto.bmp")
 * @return 見つかったときは、その場所です。
 */
std::optional<std::wstring> FindDataFile(const std::wstring& name);

/** 画像(BMP)です。 */
class Image {
public:
    Image() = default;
    ~Image();
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    /**
     * data フォルダの画像を読みます。すでに読んでいれば何もしません。
     * @return 読めたら true です。
     */
    bool Load(const std::wstring& name);

    /** 左上が (x, y) に来るように描きます。読めていなければ何もしません。 */
    void Render(HDC hdc, int x, int y) const;

private:
    HBITMAP bitmap_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};

/** 動画です。Media Foundation で開き、コマを 32 ビットの色の画像として受け取ります。 */
class Movie {
public:
    Movie() = default;
    ~Movie();
    Movie(const Movie&) = delete;
    Movie& operator=(const Movie&) = delete;

    /**
     * data フォルダの動画を開いて、最初のコマを読みます。
     * @return 開けたら true です。
     */
    bool Open(const std::wstring& name);

    void Close();

    bool is_open() const { return reader_ != nullptr; }

    /** 最後まで再生していれば true です。 */
    bool is_ended() const { return is_ended_; }

    /**
     * 再生を進めます。コマを替える時間になっていたら、次のコマを読みます。
     * @param seconds 進める時間(秒)
     */
    void Update(double seconds);

    /** 今のコマを、左上が (left, top) に来るように、コマの大きさのまま描きます。 */
    void Render(HDC hdc, int left, int top) const;

private:
    /** 次のコマを読みます。最後まで読んでいたら is_ended_ を true にします。 */
    void ReadFrame();

    IMFSourceReader* reader_ = nullptr;
    HBITMAP frame_ = nullptr;    ///< 今のコマを入れておく画像です。BitBlt で速く描けるように、画像として持ちます。
    BYTE* pixels_ = nullptr;     ///< frame_ の中身(1画素 4 バイト)
    size_t pixel_bytes_ = 0;     ///< pixels_ のバイト数
    int width_ = 0;
    int height_ = 0;
    double play_time_ = 0.0;     ///< 再生を始めてからの時間(秒)
    double next_time_ = 0.0;     ///< 次のコマに替える時間(秒)
    bool is_ended_ = false;
};
