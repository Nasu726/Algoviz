#pragma once
#include "ArrayVisualizer.hpp"
#include "GraphColors.hpp"
#include <utility>
#include <vector>

// クイックソートのビジュアライザ。
//
// 範囲の中から基準の値 (pivot) を1つ選び、それより小さいものを左へ、
// 大きいものを右へ寄せる。基準の値が入る位置はそこで決まり、左右それぞれが
// また同じやり方で並ぶ。
//
// 分け方は Lomuto の形。**右端を基準にして、左から順に1つずつ見る。**
// 基準より小さければ「小さいものの並び」の右隣と入れ替えて、その並びを伸ばす。
//
// 1ステップは4種類。
//   範囲を取り出す … 次に並べる範囲を1つ取り出し、右端を基準に決める
//   比べる         … 1つ見て、基準より小さければ左の並びへ入れ替える
//   基準を置く     … 見終わったら基準を境目へ動かす。その位置が確定する
//   何もしない     … 取り出した範囲が空か1つだけのとき
//
// 再帰は範囲の積み重ねで持つ。**空の範囲も1つだけの範囲も積んで、取り出す手を
// 踏む。** 積まずに飛ばすと、分け方が端に寄ったときに何が起きたのか見えない。
class QuickSortVisualizer : public ArrayVisualizer {
private:
    enum Phase { TakeRange, Scanning, PlacePivot };

    std::vector<std::pair<int, int>> pending; // まだ並べていない範囲
    Phase phase = TakeRange;

    int lo = -1, hi = -1; // 今の範囲
    int boundary = 0;     // ここより左は基準より小さいと分かっている
    int scanAt = 0;       // 今見ている位置
    int pivotValue = 0;
    bool skippedRange = false; // 直前の手が「何もしない」だった

    bool hasRange() const { return lo >= 0 && lo < hi; }

protected:
    void resetAlgorithm() override {
        pending.clear();
        phase = TakeRange;
        lo = hi = -1;
        boundary = scanAt = 0;
        skippedRange = false;
        focusA = focusB = -1;
        if (graph->nodeCount() <= 0) { finished = true; return; }
        pending.push_back({0, graph->nodeCount() - 1});
    }

    // 今の範囲を下地に塗ってから、確定と焦点を上に重ねる
    void syncVisuals() override {
        if (!graph) return;
        graph->resetColors();

        if (!finished && hasRange()) {
            for (int i = lo; i <= hi; i++) graph->setNodeColor(i, NODE_RANGE);
            // 基準より小さいと分かった部分
            for (int i = lo; i < boundary; i++) graph->setNodeColor(i, NODE_SMALLER);
            graph->setNodeColor(hi, NODE_FRONTIER); // 基準の値。動かすまでは右端にある
        }
        paintSettled();
        paintFocus();
    }

    bool advance() override {
        if (finished) return false;
        skippedRange = false;

        if (phase == TakeRange) {
            if (pending.empty()) { settleAll(); finished = true; return false; }

            std::pair<int, int> range = pending.back();
            pending.pop_back();
            lo = range.first;
            hi = range.second;
            focusA = focusB = -1;

            // 空か1つだけなら並べるものが無い。取り出す手だけ踏んで終わる
            if (lo >= hi) {
                if (lo == hi) markSettled(lo, lo);
                skippedRange = true;
                lo = hi = -1;
                return true;
            }

            pivotValue = valueAt(hi);
            boundary = lo;
            scanAt = lo;
            phase = Scanning;
            return true;
        }

        if (phase == Scanning) {
            focusA = scanAt;
            focusB = -1;
            if (valueAt(scanAt) < pivotValue) {
                // 小さいものの並びの右隣へ move する。同じ位置なら動かない
                swapValues(boundary, scanAt);
                focusB = boundary;
                boundary++;
            }
            scanAt++;
            if (scanAt >= hi) phase = PlacePivot;
            return true;
        }

        // 見終わったので、基準を境目へ動かす。そこが基準の入る位置
        swapValues(boundary, hi);
        focusA = boundary;
        focusB = -1;
        markSettled(boundary, boundary);

        // 右を先に積むと、左から順に取り出される
        pending.push_back({boundary + 1, hi});
        pending.push_back({lo, boundary - 1});

        lo = hi = -1;
        phase = TakeRange;
        return true;
    }

public:
    QuickSortVisualizer() {
        setValuesFrom("5 2 9 1 7 3 8 4");
    }

    emscripten::val getState(emscripten::val params) override {
        emscripten::val state = ArrayVisualizer::getState(params);
        state.set("rangeLo", hasRange() ? lo : -1);
        state.set("rangeHi", hasRange() ? hi : -1);
        // 基準の値の位置。置いたあとは確定した位置になる
        state.set("pivotIndex", hasRange() ? hi : -1);
        state.set("placingPivot", phase == PlacePivot);
        state.set("skippedRange", skippedRange);
        // まだ並べていない範囲の数。再帰がどこまで進んだかが分かる
        state.set("pendingRanges", (int)pending.size());
        return state;
    }
};
