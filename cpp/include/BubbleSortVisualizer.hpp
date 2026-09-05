#pragma once
#include "ArrayVisualizer.hpp"

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
    bool swappedInScan = false; // この走査で1回でも入れ替えたか

protected:
    void resetAlgorithm() override {
        scanEnd = graph->nodeCount() - 1;
        cursor = 0;
        swappedInScan = false;
        focusA = 0;
        focusB = graph->nodeCount() > 1 ? 1 : -1;
        if (scanEnd <= 0) { settleAll(); finished = true; } // 0個か1個なら並んでいる
    }

    bool advance() override {
        if (finished || scanEnd <= 0) { finished = true; return false; }

        focusA = cursor;
        focusB = cursor + 1;
        if (valueAt(cursor) > valueAt(cursor + 1)) {
            swapValues(cursor, cursor + 1);
            swappedInScan = true;
        }
        cursor++;

        // 走査の終わり。右端に最大の値が来たので、その位置が確定する
        if (cursor >= scanEnd) {
            markSettled(scanEnd, scanEnd);
            // 一度も入れ替えていないなら、残り全体が既に並んでいる
            scanEnd = swappedInScan ? scanEnd - 1 : 0;
            cursor = 0;
            swappedInScan = false;
            if (scanEnd <= 0) { settleAll(); finished = true; }
        }
        return true;
    }

public:
    BubbleSortVisualizer() {
        setValuesFrom("5 2 9 1 7 3 8 4");
    }
};
