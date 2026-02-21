#pragma once
#include "IVisualizer.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint> // uint8_t

class Brainfuck : public IVisualizer {
private:
    std::vector<uint8_t> tape;       // メモリ
    std::string code;                // ソースコード
    std::size_t ptr = 0;             // データポインタ
    std::size_t pc = 0;              // プログラムカウンタ
    
    std::string inputBuffer;             // 入力バッファ
    std::string outputBuffer;            // 出力バッファ
    // 変数名バッファ
    std::unordered_map<std::size_t, std::string> nameBuffer;
    std::string errorMessage;            // エラーメッセージ

    long long int stepCount = 0;               // 実行ステップ数カウント 
    bool error = false;              // エラーフラグ

    struct Snapshot {
        std::vector<uint8_t> tape;       // メモリ
        std::size_t ptr = 0;             // データポインタ
        std::size_t pc = 0;              // プログラムカウンタ
        
        std::string inputBuffer;         // 入力バッファ
        std::string outputBuffer;        // 出力バッファ
        // 変数名バッファ
        std::unordered_map<std::size_t, std::string> nameBuffer;
        long long int stepCount = 0;               // 実行ステップ数カウント 
    };

    std::vector<Snapshot> history;

    std::unordered_map<std::size_t, std::size_t> jumpTable; // ジャンプ先
    
    void analyzeJump(); // 前処理

    bool isValidCommand(char c) const;

public:
    Brainfuck();
    ~Brainfuck() override = default;

    void load(const std::string& source, const std::string& input) override;
    bool step() override;
    void stepBack() override;
    emscripten::val getState(emscripten::val params) override;
    std::string getOutput() override;
};