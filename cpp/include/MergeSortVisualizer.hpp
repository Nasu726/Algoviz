#pragma once
#include "ArrayVisualizer.hpp"
#include "GraphColors.hpp"
#include <algorithm>

// マージソートのビジュアライザ。
//
// 並んでいる2つの並びを突き合わせて、小さい方から順に取り出すと1つの並んだ
// 並びになる。長さ1の並びから始めて、幅を倍にしながら全体が1つになるまで繰り返す。
//
// **下の段が作業用の場所。** 併合した結果はいったんそこへ書き、書き終えてから
// 上の段へ戻す。ほかのソートと違って**別の場所が要る**のがマージソートの特徴で、
// その場で入れ替えるように書くと、その特徴が見えなくなる。
//
// 1ステップは3種類。
//   組を取り出す … 次に併合する2つの並びを決める
//   小さい方を移す … 2つの先頭を比べ、小さい方を下の段へ移す
//   書き戻す     … 併合が終わったら、下の段の並びを上の段へまとめて戻す
//
// 幅を倍にしていく形 (下から積む) にしてある。再帰で書くと「分ける」段階で
// 何も起きない手が続き、何をしているのか分からなくなる。
class MergeSortVisualizer : public ArrayVisualizer {
private:
    enum Phase { TakePair, Merging, CopyBack };

    Phase phase = TakePair;
    int width = 1;   // 今の段で併合する並びの長さ
    int start = 0;   // 次に取り出す組の左端

    int lo = -1, mid = -1, hi = -1; // 今の組。[lo, mid) と [mid, hi)
    int leftAt = 0, rightAt = 0;    // それぞれの先頭
    int writeAt = 0;                // 下の段の次に書く位置
    bool lonelyRun = false;         // 相方がいないので併合しない組だった

    int workOf(int i) const { return rowSize() + i; } // 下の段の同じ位置
    bool merging() const { return lo >= 0 && mid < hi; }

protected:
    // 下の段を作業用に使う。長さは配列と同じ
    int extraSlots() const override { return rowSize(); }

    void resetAlgorithm() override {
        phase = TakePair;
        width = 1;
        start = 0;
        lo = mid = hi = -1;
        leftAt = rightAt = writeAt = 0;
        lonelyRun = false;
        focusA = focusB = -1;
        if (rowSize() <= 1) { settleAll(); finished = true; }
    }

    // 今の組を下地に塗ってから、並んでいる範囲と焦点を重ねる
    void syncVisuals() override {
        if (!graph) return;
        graph->resetColors();

        if (!finished && lo >= 0) {
            for (int i = lo; i < hi; i++) graph->setNodeColor(i, NODE_RANGE);
            // 左の並びの残り。どちらから取り出しているかを分ける
            for (int i = leftAt; i < mid; i++) graph->setNodeColor(i, NODE_SMALLER);
            // 下の段に書いた部分
            for (int i = lo; i < writeAt; i++) graph->setNodeColor(workOf(i), NODE_PATH);
        }
        paintSettled();
        paintFocus();
    }

    bool advance() override {
        if (finished) return false;
        lonelyRun = false;

        if (phase == TakePair) {
            // 段の終わり。幅を倍にして最初から
            if (start >= rowSize()) {
                width *= 2;
                start = 0;
                settled.assign(rowSize(), 0); // 段が変わると「並んでいる範囲」も変わる
                if (width >= rowSize()) { settleAll(); finished = true; return false; }
            }

            lo = start;
            mid = std::min(start + width, rowSize());
            hi = std::min(start + width * 2, rowSize());
            start += width * 2;
            focusA = focusB = -1;

            // 相方がいない。この段では並べ替えるものが無い
            if (mid >= hi) {
                markSettled(lo, hi - 1);
                lonelyRun = true;
                lo = mid = hi = -1;
                return true;
            }

            leftAt = lo;
            rightAt = mid;
            writeAt = lo;
            phase = Merging;
            return true;
        }

        if (phase == Merging) {
            // 片方が尽きていたら、残っている方から取る
            bool takeLeft;
            if (leftAt >= mid)       takeLeft = false;
            else if (rightAt >= hi)  takeLeft = true;
            else                     takeLeft = valueAt(leftAt) <= valueAt(rightAt);

            int from = takeLeft ? leftAt++ : rightAt++;
            focusA = workOf(writeAt);
            focusB = -1;
            moveValue(from, workOf(writeAt));
            writeAt++;

            if (writeAt >= hi) phase = CopyBack;
            return true;
        }

        // 併合が終わった。下の段の並びを上の段へまとめて戻す
        for (int i = lo; i < hi; i++) moveValue(workOf(i), i);
        markSettled(lo, hi - 1);
        focusA = focusB = -1;
        lo = mid = hi = -1;
        phase = TakePair;
        return true;
    }

public:
    MergeSortVisualizer() {
        setValuesFrom("5 2 9 1 7 3 8 4");
    }

    emscripten::val getState(emscripten::val params) override {
        emscripten::val state = ArrayVisualizer::getState(params);
        state.set("rangeLo", merging() ? lo : -1);
        state.set("rangeMid", merging() ? mid : -1);
        state.set("rangeHi", merging() ? hi : -1);
        state.set("runWidth", width);
        state.set("copyingBack", phase == CopyBack);
        state.set("lonelyRun", lonelyRun);
        return state;
    }
};
