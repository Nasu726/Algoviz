#include "../include/Brainfuck.hpp"
#include <algorithm> // fill, max, min
#include <iostream>

using namespace emscripten;

Brainfuck::Brainfuck() {
    tape.resize(30000, 0);
}

// ジャンプ先の解析 ([ と ] の対応付け)
void Brainfuck::analyzeJump() {
    jumpTable.clear();
    std::vector<std::size_t> stack;

    for (std::size_t i = 0; i < code.length(); ++i) {
        if (code[i] == '[') {
            stack.push_back(i);
        } else if (code[i] == ']') {
            if (!stack.empty()) {
                std::size_t start = stack.back();
                stack.pop_back();
                jumpTable[start] = i;
                jumpTable[i] = start;
            }
        }
    }
}

bool Brainfuck::isValidCommand(char c) const {
    return c == '>' || c == '<' || c == '+' || c == '-' ||
           c == '[' || c == ']' || c == ',' || c == '.' ||
           c == '!';
}

void Brainfuck::load(const std::string& source, const std::string& input) {
    code = source;
    inputBuffer = input;
    outputBuffer = "";
    errorMessage = "";
    ptr = 0;
    pc = 0;
    stepCount = 0;
    error = false;
    interrupted = false;
    nameBuffer.clear();
    history.clear();

    std::fill(tape.begin(), tape.end(), 0);
    analyzeJump();

    while(pc < code.length() && !isValidCommand(code[pc])){
        pc++;
    }
}

void Brainfuck::pushHistory() {
    history.push_back({tape, ptr, pc, inputBuffer, outputBuffer, nameBuffer, stepCount});
    if (history.size() > HISTORY_LIMIT) history.erase(history.begin());
}

// 1命令だけ実行する。step() と runToEnd() の共通の中身。
// record が true のときだけ、実行前の状態を履歴に積む。
// 戻り値 true: 継続可能 / false: 終了またはエラー
bool Brainfuck::execOne(bool record) {
    if (pc >= code.length()) return false;
    if (error) return false;

    if (record) pushHistory();

    char c = code[pc];
    switch (c) {
        case '!': {
            std::size_t tmp = pc;
            pc++;
            nameBuffer[ptr] = "";
            while(pc < code.length() && code[pc] != '!'){
                if( (65 <= code[pc] && code[pc] < 91) ||  // 英大文字
                    (97 <= code[pc] && code[pc] < 123) || // 英子文字
                    code[pc] == '_' ||                    // アンダーバー
                    (nameBuffer[ptr] != "" && 48 <= code[pc] && code[pc] < 58)) // 先頭以外の数字
                    nameBuffer[ptr] += code[pc];
                pc++;
            }
            if(pc == code.length()){
                error = true;
                errorMessage = "変数宣言の！が閉じていない可能性があります";
                pc = tmp;
                return false;
            }
            break;
        }
        case '>':
            if (ptr < tape.size() - 1) ptr++;
            else {
                error = true;
                errorMessage = "139: テープの外にアクセスしようとしました (Segmentation Fault)";
                // pc はエラーを起こした命令そのものを指したままにする
                return false;
            }
            break;
        case '<':
            if (ptr > 0) ptr--;
            else {
                error = true;
                errorMessage = "139: テープの外にアクセスしようとしました (Segmentation Fault)";
                return false;
            }
            break;
        case '+': tape[ptr] = (tape[ptr] + 1) % modi; break;
        case '-': tape[ptr] = (tape[ptr] + modi - 1) % modi; break;
        case '.': outputBuffer += static_cast<char>(tape[ptr]); break;
        case ',':
            if (!inputBuffer.empty()) {
                tape[ptr] = static_cast<uint8_t>(inputBuffer.front()) % modi;
                inputBuffer.erase(0, 1);
            } else {
                tape[ptr] = modi - 1; // EOF は -1 (mod256 なら 255、mod128 なら 127)
            }
            break;
        case '[':
            if (tape[ptr] == 0 && jumpTable.count(pc)) pc = jumpTable[pc];
            break;
        case ']':
            if (tape[ptr] != 0 && jumpTable.count(pc)) pc = jumpTable[pc];
            break;
    }

    pc++;
    stepCount++;

    // 次の有効なコマンドに当たるまでスキップ
    while (pc < code.length() && !isValidCommand(code[pc])) {
        pc++;
    }

    return true;
}

bool Brainfuck::step() {
    // interrupted は直前の runToEnd がどう終わったかを表す。
    // 1ステップでも手で進めたらその情報は古くなる。
    interrupted = false;
    return execOne(true);
}

void Brainfuck::runToEnd() {
    interrupted = false;
    if (pc >= code.length() || error) return;

    // 途中経過は履歴に積まない。1ステップごとにテープ 30000 バイトを複製するため、
    // 全ステップを記録するとメモリが持たない。実行前の状態を1つだけ積んでおけば
    // 「戻る」を1回押したときに実行前へ戻れる。
    pushHistory();

    long long executed = 0;
    while (executed < RUN_STEP_LIMIT) {
        if (!execOne(false)) return;
        executed++;
    }

    // 上限に達した。WASM はメインスレッドで同期実行されるため、
    // ここで制御を返さないとタブごと固まる。
    interrupted = true;
}

void Brainfuck::stepBack() {
    if(history.empty()) return;

    Snapshot lastState = history.back();
    history.pop_back();

    tape = lastState.tape;
    ptr  = lastState.ptr;
    pc   = lastState.pc;
    inputBuffer  = lastState.inputBuffer;
    outputBuffer = lastState.outputBuffer;
    nameBuffer = lastState.nameBuffer;
    stepCount    = lastState.stepCount;

    error        = false;
    errorMessage = "";
    interrupted  = false;
}

// React側に渡す状態オブジェクトを作成
val Brainfuck::getState(val params) {
    int start = params.hasOwnProperty("start") ? params["start"].as<int>() : 0;
    int range = params.hasOwnProperty("range") ? params["range"].as<int>() : 20;

    val state = val::object();

    state.set("ptr", (int)ptr);
    state.set("pc", (int)pc);
    state.set("code", code);
    // 出力は getOutput() で取る。ここで毎回 JS 文字列へ変換すると、
    // 1ステップごとに無駄が乗るうえ、UTF-8 として不正なバイトを
    // 出力するプログラムでは変換警告が出る。
    state.set("stepCount", stepCount);
    state.set("isError", error);
    state.set("errorMessage", errorMessage);
    state.set("interrupted", interrupted);
    state.set("stepLimit", RUN_STEP_LIMIT);

    // 【軽量化】テープ全体ではなく、見えている範囲(ポインタ周辺)だけ配列にして返す
    val tapeView = val::array();

    for (int i = start; i < start+range; ++i) {
        val cell = val::object();
        cell.set("index", i);
        if(0 <= i && i < tape.size()){
            cell.set("value", tape[i]);
            cell.set("exists", true);
            if (nameBuffer.count(i) > 0){
                cell.set("name", nameBuffer[i]);

            } else {
                cell.set("name", "");
            }
        } else {
            cell.set("value", 0);
            cell.set("exists", false);
            cell.set("name", "");
        }
        tapeView.call<void>("push", cell);
    }
    state.set("tape", tapeView);

    return state;
}

std::string Brainfuck::getOutput() {
    return outputBuffer;
}

void Brainfuck::setBrainfuckModint(const bool mod256) {
    if (mod256) {
        modi = 256;
    } else {
        modi = 128;
    }
}
