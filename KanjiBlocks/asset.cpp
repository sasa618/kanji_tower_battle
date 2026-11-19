/**
 * @file asset.cpp
 * @brief data フォルダに置いた画像と動画の実装です。
 *
 * このファイルは AI(Claude)が書いたものです。
 */
#include "asset.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <system_error>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "ole32.lib")

namespace {

/**
 * 実行ファイルのフォルダから、上へいくつまでさかのぼって data フォルダを探すかです。
 * Visual Studio でビルドした実行ファイルは x64/Release にできるので、2つ上まで見ます。
 */
constexpr int kMaxParentCount = 2;

/** 読む対象(最初の映像)です。 */
constexpr DWORD kVideoStream = static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM);

/** コマの長さが分からないときに使う長さ(100 ナノ秒単位、1/30 秒)です。 */
constexpr LONGLONG kDefaultDuration = 333333;

/** 100 ナノ秒単位の時間を秒に直すときに割る数です。 */
constexpr double kTicksPerSecond = 10000000.0;

/** 処理が遅れたときに、1回の Update で読み進めるコマの数の上限です。 */
constexpr int kMaxFramesPerUpdate = 4;

/** Media Foundation を使い始めます。2回目からは何もしません。 */
bool StartMediaFoundation() {
    static const bool started = [] {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        return SUCCEEDED(MFStartup(MF_VERSION));
    }();
    return started;
}

}  // namespace

std::optional<std::wstring> FindDataFile(const std::wstring& name) {
    namespace fs = std::filesystem;
    std::error_code error;

    const fs::path in_working_folder = fs::absolute(fs::path(L"data") / name, error);
    if (!error && fs::is_regular_file(in_working_folder, error)) {
        return in_working_folder.wstring();
    }

    wchar_t module_path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, module_path, MAX_PATH);
    fs::path folder = fs::path(module_path).parent_path();

    for (int i = 0; i <= kMaxParentCount; ++i) {
        const fs::path candidate = folder / L"data" / name;
        if (fs::is_regular_file(candidate, error)) {
            return candidate.wstring();
        }
        folder = folder.parent_path();
    }

    return std::nullopt;
}

Image::~Image() {
    if (bitmap_ != nullptr) {
        DeleteObject(bitmap_);
    }
}

bool Image::Load(const std::wstring& name) {
    if (bitmap_ != nullptr) {
        return true;
    }

    const std::optional<std::wstring> path = FindDataFile(name);
    if (!path) {
        return false;
    }

    bitmap_ = static_cast<HBITMAP>(
        LoadImageW(nullptr, path->c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION));
    if (bitmap_ == nullptr) {
        return false;
    }

    BITMAP info = {};
    GetObjectW(bitmap_, sizeof(info), &info);
    width_ = info.bmWidth;
    height_ = info.bmHeight;
    return true;
}

void Image::Render(HDC hdc, int x, int y) const {
    if (bitmap_ == nullptr) {
        return;
    }

    const HDC memory_dc = CreateCompatibleDC(hdc);
    const HGDIOBJ old_bitmap = SelectObject(memory_dc, bitmap_);

    BitBlt(hdc, x, y, width_, height_, memory_dc, 0, 0, SRCCOPY);

    SelectObject(memory_dc, old_bitmap);
    DeleteDC(memory_dc);
}

Movie::~Movie() {
    Close();
}

bool Movie::Open(const std::wstring& name) {
    Close();

    const std::optional<std::wstring> path = FindDataFile(name);
    if (!path || !StartMediaFoundation()) {
        return false;
    }

    // 色の変換を任せる設定で開きます。
    IMFAttributes* attributes = nullptr;
    if (FAILED(MFCreateAttributes(&attributes, 1))) {
        return false;
    }
    attributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
    const HRESULT opened = MFCreateSourceReaderFromURL(path->c_str(), attributes, &reader_);
    attributes->Release();
    if (FAILED(opened)) {
        reader_ = nullptr;
        return false;
    }

    // 32 ビットの色で受け取ります。
    IMFMediaType* type = nullptr;
    if (FAILED(MFCreateMediaType(&type))) {
        Close();
        return false;
    }
    type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    const HRESULT converted = reader_->SetCurrentMediaType(kVideoStream, nullptr, type);
    type->Release();
    if (FAILED(converted)) {
        Close();
        return false;
    }

    // コマの大きさと、行の並ぶ向きを調べます。1行のバイト数が負なら、下の行から並んでいます。
    if (FAILED(reader_->GetCurrentMediaType(kVideoStream, &type))) {
        Close();
        return false;
    }
    UINT32 width = 0;
    UINT32 height = 0;
    MFGetAttributeSize(type, MF_MT_FRAME_SIZE, &width, &height);
    const int stride = static_cast<int>(MFGetAttributeUINT32(type, MF_MT_DEFAULT_STRIDE, width * 4));
    type->Release();
    if (width == 0 || height == 0) {
        Close();
        return false;
    }

    width_ = static_cast<int>(width);
    height_ = static_cast<int>(height);

    // コマと同じ並び方の画像を作ります。高さを負にすると、上の行から並んだ画像になります。
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width_;
    info.bmiHeader.biHeight = stride < 0 ? height_ : -height_;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    frame_ = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (frame_ == nullptr) {
        Close();
        return false;
    }
    pixels_ = static_cast<BYTE*>(bits);
    pixel_bytes_ = static_cast<size_t>(width_) * height_ * 4;
    std::memset(pixels_, 255, pixel_bytes_);

    ReadFrame();
    return true;
}

void Movie::Close() {
    if (reader_ != nullptr) {
        reader_->Release();
        reader_ = nullptr;
    }
    if (frame_ != nullptr) {
        DeleteObject(frame_);
        frame_ = nullptr;
    }
    pixels_ = nullptr;
    pixel_bytes_ = 0;
    play_time_ = 0.0;
    next_time_ = 0.0;
    is_ended_ = false;
}

void Movie::Update(double seconds) {
    if (reader_ == nullptr || is_ended_) {
        return;
    }

    play_time_ += seconds;
    for (int i = 0; i < kMaxFramesPerUpdate && !is_ended_ && play_time_ >= next_time_; ++i) {
        ReadFrame();
    }
}

void Movie::Render(HDC hdc, int left, int top) const {
    if (frame_ == nullptr) {
        return;
    }

    const HDC memory_dc = CreateCompatibleDC(hdc);
    const HGDIOBJ old_bitmap = SelectObject(memory_dc, frame_);

    BitBlt(hdc, left, top, width_, height_, memory_dc, 0, 0, SRCCOPY);

    SelectObject(memory_dc, old_bitmap);
    DeleteDC(memory_dc);
}

void Movie::ReadFrame() {
    DWORD flags = 0;
    LONGLONG time = 0;
    IMFSample* sample = nullptr;

    const HRESULT read = reader_->ReadSample(kVideoStream, 0, nullptr, &flags, &time, &sample);
    if (FAILED(read) || (flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0) {
        if (sample != nullptr) {
            sample->Release();
        }
        is_ended_ = true;
        return;
    }
    if (sample == nullptr) {
        return;
    }

    LONGLONG duration = 0;
    if (FAILED(sample->GetSampleDuration(&duration)) || duration <= 0) {
        duration = kDefaultDuration;
    }
    next_time_ = static_cast<double>(time + duration) / kTicksPerSecond;

    IMFMediaBuffer* buffer = nullptr;
    if (SUCCEEDED(sample->ConvertToContiguousBuffer(&buffer))) {
        BYTE* source = nullptr;
        DWORD length = 0;
        if (SUCCEEDED(buffer->Lock(&source, nullptr, &length))) {
            std::memcpy(pixels_, source, std::min<size_t>(length, pixel_bytes_));
            buffer->Unlock();
        }
        buffer->Release();
    }
    sample->Release();
}
