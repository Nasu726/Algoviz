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
// **見どころは、値がどちらの端から入り、どちらの端から出るか。** 3つとも
// 中身は同じで、違うのはその端だけ。分類ごとに決まるものなので setAlgorithm の
// 名前で固定し、UI に切り替えは置かない。
//
// 入れ物は箱の並び。**その両端に「外」のマスを1つずつ置いてある。** 入れる値は
// まず外に現れてから箱へ入り、取り出した値は箱から外へ出る。出た値はその手だけ
// 見えて、次の手で消える。
// 外のマスが無いと、値が箱の中に湧いて消えるだけになり、出入りの向きが見えない。
//
// スタックだけ縦に置く。**口は上で、下から積み上がる。** 横一列にすると
// キューと同じ形になり、取り出す端の色でしか違いが分からなくなる。
//
// 1ステップは1つの操作。値は入れ物の左から詰めて置く。
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
    int slots = 1; // 入れ物に入る数

    int count = 0;        // 入っている数。入れ物の 0..count-1 に詰めてある
    int opIndex = 0;      // 次に実行する操作
    int poppedCount = 0;  // 出した数
    int poppedValue = -1; // 直前の手で出した値。出していなければ -1
    bool emptyPop = false; // 直前の手が「空なのに取り出そうとした」だった
    bool overflowed = false; // 直前の手が「箱が足りずに入らなかった」だった

    // 節点は [外] [入れ物 0..slots-1] [外] の並び。
    // 縦置きのときは 0 が下、slots+1 が上になる
    int cellOf(int i) const { return i + 1; }
    int handL() const { return 0; }
    int handR() const { return slots + 1; }
    int handFor(bool left) const { return left ? handL() : handR(); }

    // 外のマスを空にする。出入りした値は、その手のあいだだけ見せる
    void clearHands() {
        if (handR() < (int)emptySlot.size()) {
            emptySlot[handL()] = 1;
            emptySlot[handR()] = 1;
        }
    }

    // 空のマスに値を置く。動いたものとして扱い、色が付くようにする
    void placeValue(int slot, int v) {
        setValueAt(slot, v);
        if (slot >= 0 && slot < (int)emptySlot.size()) emptySlot[slot] = 0;
        justSwapped = true;
    }

    // 操作の並びを流して、入れ物に要る箱の数を求める
    void computeSlots() {
        int now = 0, most = 0;
        for (const Op& op : ops) {
            if (op.push) { now++; most = std::max(most, now); }
            else if (now > 0) { now--; }
        }
        slots = std::max(1, most);
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
        values.assign(slots + 2, 0); // 箱だけ用意する。中身は実行しながら入る
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
    // スタックは縦。口が上で、下から積み上がる。
    // 横並びのときは、両端の「外」を広く取って入れ物から離す。
    // 縦のときに広げると、幅が段ごとの x を決めるので列が揃わなくなる
    void configureCells() override {
        line->setPerRow(kind == Stack ? 1 : rowSize());
        line->setBottomUp(kind == Stack);
        float hand = kind == Stack ? CELL_HALF_WIDTH : CELL_HALF_WIDTH * 2.0f;
        graph->setHalfWidth(handL(), hand);
        graph->setHalfWidth(handR(), hand);
    }

    void resetAlgorithm() override {
        count = 0;
        opIndex = 0;
        poppedCount = 0;
        poppedValue = -1;
        emptyPop = false;
        overflowed = false;
        focusA = focusB = -1;
        // 最初は入れ物も外も空
        for (int i = 0; i < (int)emptySlot.size(); i++) emptySlot[i] = 1;
        if (ops.empty()) finished = true;
    }

    // 次に出る値を塗る。ここが分類ごとの違いそのもの
    void syncVisuals() override {
        if (!graph) return;
        graph->resetColors();
        if (!finished && count > 0) {
            if (kind != Queue) graph->setNodeColor(cellOf(count - 1), NODE_FRONTIER);
            if (kind != Stack) graph->setNodeColor(cellOf(0), NODE_FRONTIER);
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
        poppedValue = -1;
        clearHands(); // 前の手で出入りした値は、ここで画面から消える

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
                overflowed = true; // 箱が足りない。何もしない
                return true;
            }
            int hand = handFor(op.left);
            placeValue(hand, op.value); // まず外に現れる
            // 左に入れるので、入っているものを右へ1つずつずらす
            if (op.left) {
                for (int i = count - 1; i >= 0; i--) moveValue(cellOf(i), cellOf(i + 1));
            }
            int dest = op.left ? cellOf(0) : cellOf(count);
            moveValue(hand, dest); // 外から箱へ入る
            focusA = dest;
            count++;
            return true;
        }

        if (count == 0) {
            emptyPop = true; // 空なので取り出せない
            return true;
        }

        int from = op.left ? cellOf(0) : cellOf(count - 1);
        int hand = handFor(op.left);
        poppedValue = valueAt(from);
        moveValue(from, hand); // 箱から外へ出る
        // 左端が空いたので、残りを左へ詰める
        if (op.left) {
            for (int i = 1; i < count; i++) moveValue(cellOf(i), cellOf(i - 1));
        }
        focusA = hand;
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
        state.set("capacity", slots);
        state.set("poppedCount", poppedCount);
        state.set("poppedValue", poppedValue);
        state.set("emptyPop", emptyPop);
        state.set("overflowed", overflowed);
        state.set("maxOps", MAX_OPS);

        // 入れ物の中身。外のマスは含めない
        emscripten::val held = emscripten::val::array();
        for (int i = 0; i < count; i++) held.call<void>("push", valueAt(cellOf(i)));
        state.set("held", held);

        // 枠を描かないマス。値が入っているときだけ見せる
        emscripten::val outside = emscripten::val::array();
        outside.call<void>("push", handL());
        outside.call<void>("push", handR());
        state.set("outsideSlots", outside);
        return state;
    }
};
