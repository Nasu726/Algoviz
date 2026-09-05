#pragma once
#include "ArrayVisualizer.hpp"
#include "GraphColors.hpp"
#include <vector>

// マージソートのビジュアライザ。
//
// 範囲を半分ずつに分けていき、1つ以下になったらそれは並んでいる。戻りながら、
// 並んでいる2つを突き合わせて小さい方から取り出すと、1つの並んだ並びになる。
//
// **下の段が作業用の場所。** 併合した結果はいったんそこへ書き、書き終えてから
// 上の段へ戻す。ほかのソートと違って**別の場所が要る**のがマージソートの特徴で、
// その場で入れ替えるように書くと、その特徴が見えなくなる。
//
// 1ステップは5種類。
//   分ける       … 範囲を半分に分け、左・右・併合の順に片付ける仕事を積む
//   1つ以下      … 分けきったので、この範囲は並んでいる
//   併合を始める … 分けた2つが並び終えたので、突き合わせに入る
//   小さい方を移す … 2つの先頭を比べ、小さい方を下の段へ移す
//   書き戻す     … 併合が終わったら、下の段の並びを上の段へまとめて戻す
//
// 再帰は仕事の積み重ねで持つ。積む順を「併合 → 右 → 左」にすると、取り出す順が
// 「左 → 右 → 併合」になり、再帰で書いたときと同じ順に進む。
class MergeSortVisualizer : public ArrayVisualizer {
private:
    // 片付ける仕事。merge が false なら「分ける」、true なら「併合する」
    struct Task {
        int lo, hi;
        bool merge;
    };

    enum Phase { Idle, Merging, CopyBack };

    std::vector<Task> pending;
    Phase phase = Idle;

    int lo = -1, mid = -1, hi = -1; // 今の範囲。[lo, mid) と [mid, hi) に分かれる
    int leftAt = 0, rightAt = 0;    // それぞれの先頭
    int writeAt = 0;                // 下の段の次に書く位置

    bool dividing = false;  // 直前の手が「分ける」だった
    bool leafRange = false; // 直前の手が「1つ以下」だった

    int workOf(int i) const { return rowSize() + i; } // 下の段の同じ位置
    bool hasRange() const { return lo >= 0; }

protected:
    // 下の段を作業用に使う。長さは配列と同じ
    int extraSlots() const override { return rowSize(); }

    void resetAlgorithm() override {
        pending.clear();
        phase = Idle;
        lo = mid = hi = -1;
        leftAt = rightAt = writeAt = 0;
        dividing = leafRange = false;
        focusA = focusB = -1;
        if (rowSize() <= 0) { finished = true; return; }
        pending.push_back({0, rowSize(), false});
    }

    // 並んでいる範囲を先に塗り、今の範囲を上に重ねる
    void syncVisuals() override {
        if (!graph) return;
        graph->resetColors();
        paintSettled();

        if (!finished && hasRange()) {
            for (int i = lo; i < mid; i++) graph->setNodeColor(i, NODE_SMALLER); // 左半分
            for (int i = mid; i < hi; i++) graph->setNodeColor(i, NODE_RANGE);   // 右半分
            for (int i = lo; i < writeAt; i++) graph->setNodeColor(workOf(i), NODE_PATH);
        }
        paintFocus();
    }

    bool advance() override {
        if (finished) return false;
        dividing = leafRange = false;

        if (phase == Merging) {
            // 片方が尽きていたら、残っている方から取る
            bool takeLeft;
            if (leftAt >= mid)      takeLeft = false;
            else if (rightAt >= hi) takeLeft = true;
            else                    takeLeft = valueAt(leftAt) <= valueAt(rightAt);

            int from = takeLeft ? leftAt++ : rightAt++;
            focusA = workOf(writeAt);
            focusB = -1;
            moveValue(from, workOf(writeAt));
            writeAt++;

            if (writeAt >= hi) phase = CopyBack;
            return true;
        }

        if (phase == CopyBack) {
            for (int i = lo; i < hi; i++) moveValue(workOf(i), i);
            markSettled(lo, hi - 1);
            focusA = focusB = -1;
            lo = mid = hi = -1;
            phase = Idle;
            return true;
        }

        // 次の仕事を取り出す
        if (pending.empty()) { settleAll(); finished = true; return false; }

        Task task = pending.back();
        pending.pop_back();
        lo = task.lo;
        hi = task.hi;
        mid = (lo + hi) / 2;
        focusA = focusB = -1;

        if (task.merge) {
            // 分けた2つが並び終えたので、突き合わせに入る
            leftAt = lo;
            rightAt = mid;
            writeAt = lo;
            phase = Merging;
            return true;
        }

        // 1つ以下なら、それだけで並んでいる
        if (hi - lo <= 1) {
            markSettled(lo, hi - 1);
            leafRange = true;
            lo = mid = hi = -1;
            return true;
        }

        // 半分に分ける。取り出す順が「左 → 右 → 併合」になるよう逆に積む
        pending.push_back({lo, hi, true});
        pending.push_back({mid, hi, false});
        pending.push_back({lo, mid, false});
        dividing = true;
        return true;
    }

public:
    MergeSortVisualizer() {
        setValuesFrom("5 2 9 1 7 3 8 4");
    }

    emscripten::val getState(emscripten::val params) override {
        emscripten::val state = ArrayVisualizer::getState(params);
        state.set("rangeLo", hasRange() ? lo : -1);
        state.set("rangeMid", hasRange() ? mid : -1);
        state.set("rangeHi", hasRange() ? hi : -1);
        state.set("dividing", dividing);
        state.set("leafRange", leafRange);
        // 次の手が書き戻しか
        state.set("copyingBack", phase == CopyBack);
        // まだ片付けていない仕事の数。再帰がどこまで進んだかが分かる
        state.set("pendingTasks", (int)pending.size());
        return state;
    }
};
