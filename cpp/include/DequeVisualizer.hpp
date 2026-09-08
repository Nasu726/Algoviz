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
// 名前で固定し、UI に切り替えは置かない。**3つとも横一列。** 形を変えると
// 出入りする端の違いだけを見比べられなくなる。
//
// **出入りするのはマスごと。** 入れ物は今入っている数だけ並び、入れる操作で
// マスが1つ外から入り、取り出す操作でマスが1つ外へ出て、次の手で消える。
// 空のマスを先に並べておくと、入れ物の大きさが最初から決まっているように見える。
//
// GraphData に節点を消す口が無いので、節点そのものは要りうる数だけ作っておき、
// 使っていないものは畳む (場所を取らず、描かない)。
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
    int slots = 1; // 入れ物に入りうる数 (節点をいくつ作るか)

    int count = 0;        // 入っている数。入れ物の 0..count-1 に詰めてある
    int opIndex = 0;      // 次に実行する操作
    int poppedCount = 0;  // 出した数
    int poppedValue = -1; // 直前の手で出した値。出していなければ -1
    int liveHand = -1;    // 外に出ているマス。無ければ -1
    bool emptyPop = false; // 直前の手が「空なのに取り出そうとした」だった

    // 節点は [外] [入れ物 0..slots-1] [外] の並び。
    // 外の2つは畳まない。出入りの起点と終点が動くと、動きが読めなくなる
    int cellOf(int i) const { return i + 1; }
    int handL() const { return 0; }
    int handR() const { return slots + 1; }
    int handFor(bool left) const { return left ? handL() : handR(); }

    // そのマスを描くか。入れ物は入っている数だけ、外は値が出ているときだけ
    bool isDrawn(int node) const {
        if (node == handL() || node == handR()) return node == liveHand;
        int i = node - 1;
        return i >= 0 && i < count;
    }

    // 使っていない入れ物のマスを畳む。今ある数だけ並んで見えるようにする
    void syncCells() {
        if (!graph || !line) return;
        std::vector<char> collapsed(rowSize(), 0);
        for (int i = count; i < slots; i++) collapsed[cellOf(i)] = 1;
        line->setCollapsed(collapsed);
        line->retarget(graph.get());
    }

    // 外のマスに値を置く
    void placeValue(int slot, int v) {
        setValueAt(slot, v);
        justSwapped = true;
    }

    // 操作の並びを流して、節点をいくつ作るか求める
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
        values.assign(slots + 2, 0); // 節点だけ用意する。中身は実行しながら入る
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
    void configureCells() override {
        line->setPerRow(rowSize());
        line->setOutside(handL(), handR());
        // 作り直した直後は空。入れ物のマスは全部畳んでおく
        std::vector<char> collapsed(rowSize(), 0);
        for (int i = 0; i < slots; i++) collapsed[cellOf(i)] = 1;
        line->setCollapsed(collapsed);
    }

    void resetAlgorithm() override {
        count = 0;
        opIndex = 0;
        poppedCount = 0;
        poppedValue = -1;
        liveHand = -1;
        emptyPop = false;
        focusA = focusB = -1;
        if (ops.empty()) finished = true;
    }

    // 次に出る値を塗る。ここが分類ごとの違いそのもの
    void syncVisuals() override {
        if (!graph) return;
        syncCells();
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
        poppedValue = -1;
        liveHand = -1; // 前の手で出たマスは、ここで画面から消える

        if (opIndex >= (int)ops.size()) {
            finished = true;
            focusA = focusB = -1;
            syncVisuals(); // この手は進まないので、色をここで塗り直す
            return false;
        }

        const Op op = ops[opIndex++];
        focusA = focusB = -1;

        if (op.push) {
            int hand = handFor(op.left);
            placeValue(hand, op.value); // まず外のマスに現れる
            // 左に入れるので、入っているものを右へ1つずつずらす
            if (op.left) {
                for (int i = count - 1; i >= 0; i--) moveValue(cellOf(i), cellOf(i + 1));
            }
            int dest = op.left ? cellOf(0) : cellOf(count);
            moveValue(hand, dest); // 外から入れ物へ入る
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
        moveValue(from, hand); // 入れ物から外へ出る
        // 左端が空いたので、残りを左へ詰める
        if (op.left) {
            for (int i = 1; i < count; i++) moveValue(cellOf(i), cellOf(i - 1));
        }
        focusA = hand;
        liveHand = hand;
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
        state.set("poppedValue", poppedValue);
        state.set("emptyPop", emptyPop);
        state.set("maxOps", MAX_OPS);

        // 入れ物の中身。外のマスは含めない
        emscripten::val held = emscripten::val::array();
        for (int i = 0; i < count; i++) held.call<void>("push", valueAt(cellOf(i)));
        state.set("held", held);

        // 描かないマス。使っていない節点は無いものとして扱う
        emscripten::val hidden = emscripten::val::array();
        for (int i = 0; i < rowSize(); i++) {
            if (!isDrawn(i)) hidden.call<void>("push", i);
        }
        state.set("hiddenSlots", hidden);
        return state;
    }
};
