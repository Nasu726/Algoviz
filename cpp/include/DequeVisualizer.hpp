#pragma once
#include "ArrayVisualizer.hpp"
#include "GraphColors.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

// スタック / キュー / デックのビジュアライザ。
//
// **上の段が入れ物、下の段が出てきた順。** 同じ操作の並びを流しても、
// 取り出す端が違えば出てくる順が変わる。**その対比がこのページの見どころ**で、
// 出た順を残さないと1手ずつ見る意味が薄い。
//
// 1ステップは1つの操作。値は左から詰めて置き、取り出すと残りが詰め直される。
//
// 入れ物の枠は先に並べておき、値の入っていない枠は空の箱として描く。
// GraphData に節点を消す口が無いので、この形でないと「減る」を表せない。
//
// 3つとも中身は同じで、**違うのは出し入れする端だけ**。分類ごとに決まるものなので
// setAlgorithm の名前で固定し、UI に切り替えは置かない。
class DequeVisualizer : public ArrayVisualizer {
public:
    // どの端から出し入れするか
    enum Kind { Stack, Queue, Deque };

    static constexpr int MAX_OPS = 20;

private:
    struct Op {
        bool push = true;  // 入れるか出すか
        bool left = false; // 左の端か右の端か
        int value = 0;     // 入れる値
    };

    Kind kind = Stack;
    std::vector<Op> ops;
    int slots = 1; // 上下それぞれの枠の数

    int count = 0;        // 入っている数。上の段の 0..count-1 に詰めてある
    int opIndex = 0;      // 次に実行する操作
    int poppedCount = 0;  // 出した数。下の段の 0..poppedCount-1 に並べてある
    bool emptyPop = false; // 直前の手が「空なのに取り出そうとした」だった
    bool overflowed = false; // 直前の手が「枠が足りずに入らなかった」だった

    int workOf(int i) const { return rowSize() + i; } // 下の段の同じ位置

    // 空の枠に値を置く。動いたものとして扱い、色が付くようにする
    void placeValue(int slot, int v) {
        setValueAt(slot, v);
        if (slot >= 0 && slot < (int)emptySlot.size()) emptySlot[slot] = 0;
        justSwapped = true;
    }

    // 操作の並びを流して、上下の段に要る枠の数を求める
    void computeSlots() {
        int now = 0, most = 0, pops = 0;
        for (const Op& op : ops) {
            if (op.push) { now++; most = std::max(most, now); }
            else if (now > 0) { now--; pops++; }
        }
        slots = std::max(1, std::max(most, pops));
    }

    static std::string lowered(const std::string& s) {
        std::string out = s;
        for (char& c : out) c = (char)std::tolower((unsigned char)c);
        return out;
    }

    // "push 5 pop pushL 3 popR" のような並びを読む
    void setOpsFrom(const std::string& text) {
        ops.clear();
        std::istringstream iss(text);
        std::string word;
        while (iss >> word && (int)ops.size() < MAX_OPS) {
            std::string w = lowered(word);
            Op op;
            if (w == "push" || w == "pushl" || w == "pushr") {
                int v = 0;
                if (!(iss >> v)) break; // 値が続いていない
                op.push = true;
                op.left = (w == "pushl");
                op.value = v;
            } else if (w == "pop" || w == "popl" || w == "popr") {
                op.push = false;
                op.left = (w == "popl");
            } else {
                continue; // 読めない語は飛ばす
            }
            // 端は分類で決まる。スタックとキューでは書かれた向きを使わない
            if (kind == Stack)      op.left = false;
            else if (kind == Queue) op.left = !op.push; // 入れるのは右、出すのは左
            ops.push_back(op);
        }
        computeSlots();
        values.assign(slots, 0); // 枠だけ用意する。中身は実行しながら入る
        resetRun();
    }

    // 空なのに取り出す操作が出ないように作る
    void generateOps(int howMany) {
        howMany = std::clamp(howMany, 1, MAX_OPS);
        std::string text;
        int now = 0;
        for (int i = 0; i < howMany; i++) {
            bool push = (now == 0) || (randInt(2) == 0);
            if (i == howMany - 1 && now > 0) push = false; // 最後は出して終わる
            if (push) {
                text += (kind == Deque ? (randInt(2) ? "pushR " : "pushL ") : "push ");
                text += std::to_string(randInt(99) + 1) + " ";
                now++;
            } else {
                text += (kind == Deque ? (randInt(2) ? "popR " : "popL ") : "pop ");
                now--;
            }
        }
        setOpsFrom(text);
    }

protected:
    // 下の段を「出てきた順」に使う
    int extraSlots() const override { return rowSize(); }

    void resetAlgorithm() override {
        count = 0;
        opIndex = 0;
        poppedCount = 0;
        emptyPop = false;
        overflowed = false;
        focusA = focusB = -1;
        // 入れ物は最初、全部の枠が空
        for (int i = 0; i < (int)emptySlot.size(); i++) emptySlot[i] = 1;
        if (ops.empty()) finished = true;
    }

    // 次に出る値と、もう出た値を塗る
    void syncVisuals() override {
        if (!graph) return;
        graph->resetColors();

        for (int i = 0; i < poppedCount; i++) graph->setNodeColor(workOf(i), NODE_VISITED);

        // 取り出す端。ここが分類ごとの違いそのもの
        if (!finished && count > 0) {
            if (kind != Queue) graph->setNodeColor(count - 1, NODE_FRONTIER);
            if (kind != Stack) graph->setNodeColor(0, NODE_FRONTIER);
        }
        paintFocus();
    }

    bool handleCommand(const std::string& source, const std::string& input) override {
        // 入力欄には値ではなく操作の並びが入る
        if (source == "setValues") { setOpsFrom(input); return true; }
        if (source == "resetRun")  { resetRun(); return true; }
        if (source == "genRandom") {
            int howMany = 12;
            std::istringstream iss(input);
            iss >> howMany;
            generateOps(howMany);
            return true;
        }
        return false;
    }

    bool advance() override {
        if (finished) return false;
        emptyPop = false;
        overflowed = false;

        if (opIndex >= (int)ops.size()) {
            finished = true;
            focusA = focusB = -1;
            syncVisuals(); // この手は進まないので、色をここで塗り直す
            return false;
        }

        const Op op = ops[opIndex++];
        focusA = focusB = -1;

        if (op.push) {
            if (count >= slots) {
                overflowed = true; // 枠が足りない。何もしない
            } else if (op.left) {
                // 左に入れるので、入っているものを右へ1つずつずらす
                for (int i = count - 1; i >= 0; i--) moveValue(i, i + 1);
                placeValue(0, op.value);
                focusA = 0;
                count++;
            } else {
                placeValue(count, op.value);
                focusA = count;
                count++;
            }
            return true;
        }

        if (count == 0) {
            emptyPop = true; // 空なので取り出せない
            return true;
        }

        if (op.left) {
            moveValue(0, workOf(poppedCount));
            // 左端が空いたので、残りを左へ詰める
            for (int i = 1; i < count; i++) moveValue(i, i - 1);
        } else {
            moveValue(count - 1, workOf(poppedCount));
        }
        focusA = workOf(poppedCount);
        poppedCount++;
        count--;
        return true;
    }

public:
    explicit DequeVisualizer(Kind k) : kind(k) {
        setOpsFrom(k == Deque ? "pushR 5 pushL 2 pushR 9 popL popR popL"
                              : "push 5 push 2 pop push 9 pop pop");
    }

    emscripten::val getState(emscripten::val params) override {
        emscripten::val state = ArrayVisualizer::getState(params);

        // 操作の並び。パネルが今どれを実行したかを示す
        emscripten::val list = emscripten::val::array();
        for (const Op& op : ops) {
            std::string label = op.push ? "push" : "pop";
            if (kind == Deque) label += op.left ? "L" : "R";
            if (op.push) label += " " + std::to_string(op.value);
            list.call<void>("push", label);
        }
        state.set("ops", list);
        state.set("opIndex", opIndex);
        state.set("heldCount", count);
        state.set("poppedCount", poppedCount);
        state.set("emptyPop", emptyPop);
        state.set("overflowed", overflowed);
        state.set("maxOps", MAX_OPS);
        return state;
    }
};
