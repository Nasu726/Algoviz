#pragma once
#include "SearchVisualizer.hpp"

// 線形探索のビジュアライザ。
//
// 左から順に1つずつ見て、探す値と同じなら止まる。最後まで見て同じものが
// 無ければ、その配列には無い。
//
// **1ステップは「1つ見る」。** 見終わった位置は灰色になるので、灰色が左から
// 伸びていく長さがそのまま「何回見たか」になる。
//
// 並んでいる必要は無い。どんな並びでも同じ手順で探せるのが二分探索との違いで、
// そのぶん端まで見ることがある。
class LinearSearchVisualizer : public SearchVisualizer {
private:
    int cursor = 0; // 次に見る位置

protected:
    void resetAlgorithm() override {
        SearchVisualizer::resetAlgorithm();
        cursor = 0;
        focusA = focusB = -1;
    }

    bool advance() override {
        if (finished) return false;

        // 見る場所が無くなった。この配列には無い。
        // この手は進まないので step() が色を塗り直さない。ここで塗っておく
        if (cursor >= rowSize()) {
            finished = true;
            focusA = focusB = -1;
            settleAll();
            syncVisuals();
            return false;
        }

        focusA = cursor;
        focusB = -1;
        if (valueAt(cursor) == target) {
            foundAt = cursor;
            finished = true;
            return true;
        }

        markSettled(cursor, cursor); // ここは違ったので、もう見ない
        cursor++;
        return true;
    }

public:
    LinearSearchVisualizer() {
        target = 7;
        setValuesFrom("5 2 9 1 7 3 8 4");
    }

    emscripten::val getState(emscripten::val params) override {
        emscripten::val state = SearchVisualizer::getState(params);
        // 次に見る位置。見つけて止まったあとは -1
        state.set("cursor", finished ? -1 : cursor);
        return state;
    }
};
