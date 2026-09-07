#pragma once
#include "SearchVisualizer.hpp"
#include <algorithm>
#include <numeric>

// 二分探索のビジュアライザ。
//
// 探す範囲の真ん中を見て、探す値と比べる。同じなら見つかり、違えば**半分を
// 丸ごと捨てられる。** それを繰り返すと範囲がすぐに無くなる。
//
// **1ステップは「真ん中を見て、範囲を半分にする」。**
//
// **並んでいることが前提。** 並んでいない入力でも動かせるようにしてあるのは、
// そのとき「在るのに見つからない」ことが起きるのを見せるため。勝手に並べ替えると
// その前提が見えなくなる。ランダム生成だけは昇順で作る。
class BinarySearchVisualizer : public SearchVisualizer {
private:
    int lo = 0, hi = -1; // 探す範囲。hi < lo なら空
    int mid = -1;        // 今見ている位置

    bool hasRange() const { return lo <= hi; }

protected:
    void resetAlgorithm() override {
        SearchVisualizer::resetAlgorithm();
        lo = 0;
        hi = rowSize() - 1;
        mid = -1;
        focusA = focusB = -1;
    }

    // 探す範囲を下地に塗ってから、見終わった範囲と今見ている位置を重ねる
    void syncVisuals() override {
        if (!graph) return;
        graph->resetColors();
        if (!finished && hasRange()) {
            for (int i = lo; i <= hi; i++) graph->setNodeColor(i, NODE_RANGE);
        }
        paintSettled();
        paintFocus();
        if (foundAt >= 0) graph->setNodeColor(foundAt, NODE_PATH);
    }

    // ランダム生成は昇順で作る。並んでいないと二分探索の前提が崩れる
    bool handleCommand(const std::string& source, const std::string& input) override {
        if (source == "genRandom") {
            int count = 12;
            std::istringstream iss(input);
            iss >> count;
            generateValues(count);
            std::sort(values.begin(), values.end());
            resetRun();
            return true;
        }
        return SearchVisualizer::handleCommand(source, input);
    }

    bool advance() override {
        if (finished) return false;

        // 範囲が空になった。この配列には無い。
        // この手は進まないので step() が色を塗り直さない。ここで塗っておく
        if (!hasRange()) {
            finished = true;
            mid = -1;
            focusA = focusB = -1;
            settleAll();
            syncVisuals();
            return false;
        }

        mid = (lo + hi) / 2;
        focusA = mid;
        focusB = -1;

        if (valueAt(mid) == target) {
            foundAt = mid;
            finished = true;
            return true;
        }

        if (valueAt(mid) < target) {
            markSettled(lo, mid); // 真ん中より小さい側は、もう見ない
            lo = mid + 1;
        } else {
            markSettled(mid, hi); // 真ん中より大きい側は、もう見ない
            hi = mid - 1;
        }
        return true;
    }

public:
    BinarySearchVisualizer() {
        target = 7;
        setValuesFrom("1 2 3 4 5 7 8 9");
    }

    emscripten::val getState(emscripten::val params) override {
        emscripten::val state = SearchVisualizer::getState(params);
        state.set("rangeLo", (!finished && hasRange()) ? lo : -1);
        state.set("rangeHi", (!finished && hasRange()) ? hi : -1);
        // 今見ている真ん中。見終わったあとは -1
        state.set("midIndex", finished ? -1 : mid);
        // まだ見ていない場所の数
        state.set("rangeSize", hasRange() ? hi - lo + 1 : 0);
        return state;
    }
};
