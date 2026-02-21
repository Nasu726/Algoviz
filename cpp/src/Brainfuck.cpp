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
    nameBuffer.clear();
    history.clear();

    std::fill(tape.begin(), tape.end(), 0);
    analyzeJump();

    while(pc < code.length() && !isValidCommand(code[pc])){
        pc++;
    }
}

bool Brainfuck::step() {
    if (pc >= code.length()) return false;
    if (error) return false;

    history.push_back({tape, ptr, pc, inputBuffer, outputBuffer, nameBuffer, stepCount});
    if (history.size() > 1000) history.erase(history.begin());
    char c = code[pc];
    switch (c) {
        case '!': {
            int tmp = pc;
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
                --pc;
                return false;
            }
            break;
        case '<':
            if (ptr > 0) ptr--;
            else {
                error = true;
                errorMessage = "139: テープの外にアクセスしようとしました (Segmentation Fault)";
                --pc;
                return false;
            }
            break;
        case '+': tape[ptr]++; break;
        case '-': tape[ptr]--; break;
        case '.': outputBuffer += static_cast<char>(tape[ptr]); break;
        case ',':
            if (!inputBuffer.empty()) {
                tape[ptr] = static_cast<uint8_t>(inputBuffer.front());
                inputBuffer.erase(0, 1);
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
}

// React側に渡す状態オブジェクトを作成
val Brainfuck::getState(val params) {
    int start = params.hasOwnProperty("start") ? params["start"].as<int>() : 0;
    int range = params.hasOwnProperty("range") ? params["range"].as<int>() : 20;
    
    val state = val::object();

    state.set("ptr", (int)ptr);
    state.set("pc", (int)pc);
    state.set("code", code);
    state.set("output", outputBuffer);
    state.set("stepCount", stepCount);
    state.set("isError", error);
    state.set("errorMessage", errorMessage);

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
