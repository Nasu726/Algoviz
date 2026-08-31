#pragma once
#include <string>
#include <emscripten/val.h>

class IVisualizer {
public:
    virtual ~IVisualizer() = default;

    // ソースコードと入力を読み込む
    virtual void load(const std::string& source, const std::string& input) = 0;

    // 事前計算を1単位だけ進める。準備が完了していれば true。
    //
    // step() が「アルゴリズムの1手」を意味するのに対し、こちらは
    // 「表示できる状態にするための下ごしらえ」を指す。
    // Brainfuck は下ごしらえが不要なので常に true。
    // グラフ系はレイアウトの収束計算をここで進める。
    // 描画ループ (60fps) が回すのはこちらで、step() は再生コントロールが叩く。
    virtual bool prepare() { return true; }

    // 1ステップ実行 (true: 継続, false: 終了/エラー)
    virtual bool step() = 0;

    virtual void runToEnd() { while (step()) {} }

    // ステップバック。戻せないビジュアライザは何もしない。
    virtual void stepBack() {}

    // 現在の状態をJSオブジェクトとして返す
    virtual emscripten::val getState(emscripten::val params) = 0;

    // 出力を返す
    virtual std::string getOutput() { return ""; }
};
