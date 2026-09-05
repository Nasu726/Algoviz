#pragma once
#include "ArrayVisualizer.hpp"

// 挿入ソートのビジュアライザ。
//
// 左側は並んだ状態を保ち、次の値を左へ送りながら入る場所を探す。
// 1ステップは「左隣と1回比べる」。大小が逆ならその手で入れ替える。
//
// **灰色の範囲は「並んでいる」だけで、位置が確定したわけではない。**
// バブルや選択ソートの確定とは意味が違う (後から来た値が割り込む)。
class InsertionSortVisualizer : public ArrayVisualizer {
private:
    int next = 1;   // これから入れる値の位置。ここより左は並んでいる
    int cursor = 1; // 今 cursor-1 と cursor を比べている

protected:
    void resetAlgorithm() override {
        next = 1;
        cursor = 1;
        focusA = focusB = -1;
        markSettled(0, 0); // 1つだけなら並んでいる
        if (graph->nodeCount() <= 1) { settleAll(); finished = true; }
    }

    bool advance() override {
        if (finished) return false;

        // 左隣より小さいうちは、入れ替えながら左へ送る
        if (cursor > 0 && valueAt(cursor - 1) > valueAt(cursor)) {
            focusA = cursor - 1;
            focusB = cursor;
            swapValues(cursor - 1, cursor);
            cursor--;
            return true;
        }

        // 左隣以上なので、ここがこの値の入る場所
        focusA = cursor;
        focusB = cursor > 0 ? cursor - 1 : -1;
        next++;
        markSettled(0, next - 1);
        if (next >= graph->nodeCount()) {
            finished = true;
        } else {
            cursor = next;
        }
        return true;
    }

public:
    InsertionSortVisualizer() {
        setValuesFrom("5 2 9 1 7 3 8 4");
    }

    emscripten::val getState(emscripten::val params) override {
        emscripten::val state = ArrayVisualizer::getState(params);
        // これから入れる値。並んでいる範囲の右隣
        state.set("insertingAt", finished ? -1 : next);
        return state;
    }
};
