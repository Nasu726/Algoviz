#pragma once
#include "ArrayVisualizer.hpp"
#include "GraphColors.hpp"
#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

// バケットソートのビジュアライザ。
//
// **日本語で言うバケットソート**: 値ごとの置き場 (頻度配列) を用意し、値を数えて
// から添字の順に展開する。比べる手は1つも無い。英語圏で counting sort と呼ばれる
// もので、英語圏の bucket sort (範囲で分けて中を挿入ソート) とは別物。
// このアプリは日本語 UI なので、名前は日本語の慣習に合わせる。
//
// **上の段が配列、下の段が頻度配列。** 頻度配列の添字がそのまま値で、上に見出しを
// 書く。値の範囲は 0〜9 に絞る。頻度配列は値の数だけ横に並ぶので、これ以上広いと
// 画面に収めたときに数字が読めなくなる。
//
// 2つの局面を順にたどる。
//   数える   … 配列の左から1つ見て、その値の頻度を +1。数えたマスは空く
//   展開する … 頻度配列を左から1つずつ見て、頻度のぶんだけ値を配列へ書き出す。
//              頻度が 0 の添字も1手かけて見る (ループは全部の添字を訪れる)
class BucketSortVisualizer : public ArrayVisualizer {
public:
    static constexpr int RANGE = 10; // 値は 0 〜 RANGE-1

private:
    enum Phase { Count, Expand };

    Phase phase = Count;
    int next = 0;        // 数える: 次に見る配列の位置 / 展開する: 次に書く位置
    int cursor = 0;      // 展開する: 今見ている頻度配列の添字
    int remain = 0;      // 展開する: 今の添字であと何個書くか
    bool looked = false; // 展開する: 今の添字を1手以上見たか

    // 直前の手が何だったか
    int counted = -1;     // 数えた値。無ければ -1
    int written = -1;     // 書き出した値。無ければ -1
    bool sawZero = false; // 頻度 0 の添字を見た

    // 節点は2段。1段の幅は配列の数と RANGE の大きい方で、余りは描かない。
    //   1段目: [配列 0..n-1] [余り]
    //   2段目: [余り] [頻度 0..RANGE-1] [余り]   … 余りを両側に分けて配列の下の真ん中に置く
    // **配列の数と値の範囲は無関係。** 幅を RANGE に固定すると、配列が RANGE より
    // 長いときに頻度配列の節点が配列の後半に重なる
    int width() const { return std::max(rowSize(), RANGE); }
    int countPad() const { return (width() - RANGE) / 2; }
    int countSlot(int v) const { return width() + countPad() + v; }
    int countOf(int v) const { return valueAt(countSlot(v)); }
    void setCount(int v, int c) { setValueAt(countSlot(v), c); }

    // 範囲の外の値は端に寄せる。頻度配列に置き場が無い
    static int clampValue(int v) { return std::clamp(v, 0, RANGE - 1); }

    void countOne() {
        int v = valueAt(next);
        setCount(v, countOf(v) + 1);
        emptySlot[next] = 1; // 数えた値は頻度に吸われて、マスが空く
        counted = v;
        focusA = countSlot(v);
        justSwapped = true; // 増えたマスを緑で塗る
        next++;
    }

    void writeOne() {
        setValueAt(next, cursor);
        emptySlot[next] = 0;
        setCount(cursor, countOf(cursor) - 1);
        written = cursor;
        focusA = next;
        justSwapped = true;
        remain--;
        looked = true;
        next++;
        markSettled(0, next - 1);
    }

    // 最後の添字まで見終えたら終わり。全部書き出していても、残りの添字は見る
    // (ループは全部の添字を訪れる)
    void finishIfLastIndex() {
        if (cursor == RANGE - 1 && remain == 0) finished = true;
    }

    void enterCursor(int v) {
        cursor = v;
        remain = v < RANGE ? countOf(v) : 0;
        looked = false;
    }

protected:
    // 2段ぶんから配列の数を引いたもの
    int extraSlots() const override { return 2 * width() - rowSize(); }

    void configureCells() override { line->setPerRow(width()); }

    void resetAlgorithm() override {
        phase = Count;
        next = 0;
        cursor = remain = 0;
        looked = false;
        counted = written = -1;
        sawZero = false;
        focusA = focusB = -1;
        // 配列より後ろのマスは「空」ではない。余りは描かず、頻度は 0 が入っている
        for (int i = rowSize(); i < (int)emptySlot.size(); i++) emptySlot[i] = 0;
        for (int v = 0; v < RANGE; v++) setCount(v, 0);
        if (rowSize() <= 0) finished = true;
    }

    void syncVisuals() override {
        if (!graph) return;
        graph->resetColors();
        paintSettled();
        paintFocus();
        if (finished) return;
        // 展開中に見ている添字
        if (phase == Expand && cursor < RANGE) {
            graph->setNodeColor(countSlot(cursor), NODE_VISITING);
        }
    }

    bool handleCommand(const std::string& source, const std::string& input) override {
        // 値を範囲に寄せてから受け取る
        if (source == "setValues") {
            values.clear();
            std::istringstream iss(input);
            int v;
            while (iss >> v && (int)values.size() < MAX_VALUES) values.push_back(clampValue(v));
            resetRun();
            return true;
        }
        // 同じ値が何個あっても頻度が増えるだけ、を見せたいので重複を許す
        if (source == "genRandom") {
            int count = 12;
            std::istringstream iss(input);
            iss >> count;
            count = std::clamp(count, 1, MAX_VALUES);
            values.clear();
            for (int i = 0; i < count; i++) values.push_back(randInt(RANGE));
            resetRun();
            return true;
        }
        return ArrayVisualizer::handleCommand(source, input);
    }

    bool advance() override {
        if (finished) return false;
        counted = written = -1;
        sawZero = false;
        focusA = focusB = -1;

        // 局面の切り替えは手を消費しない。見えることが起きる手まで進める
        for (;;) {
            switch (phase) {
            case Count:
                if (next < rowSize()) { countOne(); return true; }
                phase = Expand;
                next = 0;
                enterCursor(0);
                break;

            case Expand:
                if (cursor >= RANGE) {
                    // finishIfLastIndex が先に立てるので、ここへ来るのは配列が空のときだけ
                    finished = true;
                    syncVisuals();
                    return false;
                }
                if (remain > 0) {
                    writeOne();
                    finishIfLastIndex();
                    return true;
                }
                if (!looked) {
                    // 頻度 0 の添字を見る。何も書かないが、1手かける
                    looked = true;
                    sawZero = true;
                    finishIfLastIndex();
                    return true;
                }
                enterCursor(cursor + 1);
                break;
            }
        }
    }

public:
    BucketSortVisualizer() {
        // 同じ値を混ぜてある。頻度が 2 以上になるところが見どころ
        setValuesFrom("7 3 9 7 0 2 3 5 7 1");
    }

    emscripten::val getState(emscripten::val params) override {
        emscripten::val state = ArrayVisualizer::getState(params);

        state.set("phase", phase == Count ? "count" : "expand");
        state.set("range", RANGE);
        state.set("counted", counted);
        state.set("written", written);
        state.set("sawZero", sawZero);
        state.set("expandAt", phase == Expand && !finished ? cursor : -1);
        state.set("countedTotal", phase == Count ? next : rowSize());

        // 各段の余りは無いものとして扱う
        emscripten::val hidden = emscripten::val::array();
        for (int i = rowSize(); i < width(); i++) hidden.call<void>("push", i);
        for (int i = width(); i < countSlot(0); i++) hidden.call<void>("push", i);
        for (int i = countSlot(RANGE - 1) + 1; i < 2 * width(); i++) hidden.call<void>("push", i);
        state.set("hiddenSlots", hidden);

        // 頻度配列の見出し。添字がそのまま値
        emscripten::val labels = emscripten::val::array();
        for (int v = 0; v < RANGE; v++) {
            emscripten::val l = emscripten::val::object();
            std::size_t o = (std::size_t)countSlot(v) * GraphData::NODE_STRIDE;
            l.set("x", graph->nodeData[o]);
            l.set("y", LineLayout::ROW_GAP - CELL_HALF_WIDTH - 12.0f);
            l.set("text", std::to_string(v));
            l.set("align", "center");
            labels.call<void>("push", l);
        }
        state.set("labels", labels);
        return state;
    }
};
