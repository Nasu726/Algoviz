#pragma once
#include <string>
#include <emscripten/val.h>

class IVisualizer {
public:
    virtual ~IVisualizer() = default;

    // ソースコードと入力を読み込む
    virtual void load(const std::string& source, const std::string& input) = 0;

    // 1ステップ実行 (true: 継続, false: 終了/エラー)
    virtual bool step() = 0;

    // ステップバック
    virtual void stepBack() = 0;

    // 現在の状態をJSオブジェクトとして返す
    virtual emscripten::val getState(emscripten::val params) = 0;
    
    // 出力を返す
    virtual std::string getOutput() = 0;

};