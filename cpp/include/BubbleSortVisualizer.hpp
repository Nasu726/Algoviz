#pragma once
#include "ArrayVisualizer.hpp"

// バブルソートのビジュアライザ。
//
// **1ステップは「隣どうしを1回比べる」。** 大小が逆ならその手で入れ替える。
// 比べるのと入れ替えるのを別の手にすると手数が倍になり、隣どうしを何度も
// 比べるソートでは1手ずつ追うには多すぎる。
//
// 1回の走査で最大の値が右端まで運ばれるので、右から順に位置が確定していく。
//
// **速さの工夫はしていない。**
//   - 入れ替えが起きなくなっても打ち切らない
//   - 確定した範囲も走査から外さず、毎回、左端から右端まで見る
// どちらも速くはなるが、「隣どうしを何度も比べて少しずつ運ぶ」という仕組み
// そのものが見えにくくなる。手数は並びによらず (n-1)^2 で一定になる。
class BubbleSortVisualizer : public ArrayVisualizer {
private:
    int pass = 0;     // 何周目か
    int cursor = 0;   // 次に比べる位置

    int lastPair() const { return graph->nodeCount() - 2; } // 比べる位置の右端

protected:
    void resetAlgorithm() override {
        pass = 0;
        cursor = 0;
        focusA = 0;
        focusB = graph->nodeCount() > 1 ? 1 : -1;
        // 0個か1個なら並んでいる
        if (graph->nodeCount() <= 1) { settleAll(); finished = true; }
    }

    bool advance() override {
        if (finished) return false;

        focusA = cursor;
        focusB = cursor + 1;
        if (valueAt(cursor) > valueAt(cursor + 1)) swapValues(cursor, cursor + 1);
        cursor++;

        // 走査の終わり。右端に最大の値が来たので、その位置が確定する
        if (cursor > lastPair()) {
            markSettled(lastPair() + 1 - pass, lastPair() + 1 - pass);
            pass++;
            cursor = 0;
            // 周の数は n-1。残った左端はほかが確定した時点で決まっている
            if (pass > lastPair()) { settleAll(); finished = true; }
        }
        return true;
    }

public:
    BubbleSortVisualizer() {
        setValuesFrom("5 2 9 1 7 3 8 4");
    }

    emscripten::val getState(emscripten::val params) override {
        emscripten::val state = ArrayVisualizer::getState(params);
        state.set("pass", pass);
        return state;
    }
};
