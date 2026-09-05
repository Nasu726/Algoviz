#pragma once
#include "ArrayVisualizer.hpp"
#include "GraphColors.hpp"

// バブルソートのビジュアライザ。
//
// **1ステップは「隣どうしを1回比べる」。** 大小が逆ならその手で入れ替える。
// 比べるのと入れ替えるのを別の手にすると手数が倍になり、隣どうしを何度も
// 比べるソートでは1手ずつ追うには多すぎる。
//
// 1回の走査で最大の値が右端まで運ばれるので、右から順に位置が確定していく。
// 走査中に一度も入れ替えが起きなければ、そこで全体が並んでいる。
class BubbleSortVisualizer : public ArrayVisualizer {
private:
    int scanEnd = 0;   // この位置までを走査する。ここより右は確定済み
    int cursor  = 0;   // 次に比べる位置
    int pairLeft = 0;  // 直前に比べた2つの左側
    bool swappedInScan = false; // この走査で1回でも入れ替えたか
    bool justSwapped = false;   // 直前の手で入れ替えたか

protected:
    void resetAlgorithm() override {
        scanEnd = graph->nodeCount() - 1;
        cursor = 0;
        pairLeft = 0;
        swappedInScan = false;
        justSwapped = false;
        if (scanEnd <= 0) finished = true; // 0個か1個なら並んでいる
    }

    void syncVisuals() override {
        if (!graph) return;
        graph->resetColors();

        if (finished) {
            for (int i = 0; i < graph->nodeCount(); i++) graph->setNodeColor(i, NODE_VISITED);
            return;
        }

        // 確定した範囲。もう動かないので目立たせない
        for (int i = scanEnd + 1; i < graph->nodeCount(); i++) {
            graph->setNodeColor(i, NODE_VISITED);
        }

        // 入れ替えたときは動いた先の2つ、そうでなければ比べた2つ
        int color = justSwapped ? NODE_PATH : NODE_VISITING;
        graph->setNodeColor(pairLeft, color);
        graph->setNodeColor(pairLeft + 1, color);
    }

    bool advance() override {
        if (finished || scanEnd <= 0) { finished = true; return false; }

        pairLeft = cursor;
        justSwapped = valueAt(cursor) > valueAt(cursor + 1);
        if (justSwapped) {
            swapValues(cursor, cursor + 1);
            swappedInScan = true;
        }
        cursor++;

        // 走査の終わり。右端に最大の値が来たので、その位置が確定する
        if (cursor >= scanEnd) {
            // 一度も入れ替えていないなら、残り全体が既に並んでいる
            scanEnd = swappedInScan ? scanEnd - 1 : 0;
            cursor = 0;
            swappedInScan = false;
            if (scanEnd <= 0) finished = true;
        }
        return true;
    }

public:
    BubbleSortVisualizer() {
        setValuesFrom("5 2 9 1 7 3 8 4");
    }

    emscripten::val getState(emscripten::val params) override {
        emscripten::val state = ArrayVisualizer::getState(params);
        state.set("compareLeft", finished ? -1 : pairLeft);
        state.set("swapped", justSwapped);
        // ここより右は位置が確定している
        state.set("sortedFrom", finished ? 0 : scanEnd + 1);
        return state;
    }
};
