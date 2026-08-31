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

    long long int stepCount = 0;     // 実行ステップ数カウント
    int modi = 256;                  // セルの値の範囲を決定
    bool error = false;              // エラーフラグ
    bool interrupted = false;        // runToEnd がステップ上限で打ち切られた

    // runToEnd の1回あたりのステップ上限。
    // WASM はメインスレッドで同期実行されるため、停止しないプログラム
    // (例: "+[]" や EOF=-1 での ",[.,]") を無制限に回すとタブごと固まる。
    static constexpr long long RUN_STEP_LIMIT = 50000000;

    // 保持するスナップショットの数。1つあたりテープ 30000 バイトを複製する。
    static constexpr std::size_t HISTORY_LIMIT = 1000;

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

    void pushHistory(); // 現在の状態をスナップショットとして積む

    // 1命令だけ実行する。step() と runToEnd() の共通の中身。
    // record が true のときだけ実行前の状態を履歴に積む。
    bool execOne(bool record);

public:
    Brainfuck();
    ~Brainfuck() override = default;

    void load(const std::string& source, const std::string& input) override;
    bool step() override;
    void runToEnd() override;
    void stepBack() override;
    emscripten::val getState(emscripten::val params) override;
    std::string getOutput() override;

    void setBrainfuckModint(const bool mod256);
};