#pragma once
#include "ArrayVisualizer.hpp"

// シェーカーソート (双方向のバブルソート) のビジュアライザ。
//
// 隣どうしを比べる単位はバブルソートと同じで、走査の向きが1周ごとに入れ替わる。
// 右へ走ると最大の値が右端へ、左へ走ると最小の値が左端へ運ばれるので、
// **両端から確定していく。**
//
// バブルソートが苦手な「小さい値が右端近くにある」並びで差が出る。
// バブルは1周で1つしか左へ動かせないが、左向きの走査なら一気に運べる。
class ShakerSortVisualizer : public ArrayVisualizer {
private:
    int left = 0;      // ここより左は確定済み
    int right = 0;     // ここより右は確定済み
    int cursor = 0;    // 今 cursor と cursor+1 を比べている
    bool movingRight = true;
    bool swappedInScan = false;

    // 端を1つ確定させ、走査の向きを変える。並び終えていたら true
    bool turnAround() {
        if (movingRight) {
            markSettled(right, right);
            right--;
        } else {
            markSettled(left, left);
            left++;
        }
        // 一度も入れ替えていないなら、残り全体が既に並んでいる
        if (!swappedInScan || left >= right) return true;

        movingRight = !movingRight;
        cursor = movingRight ? left : right - 1;
        swappedInScan = false;
        return false;
    }

protected:
    void resetAlgorithm() override {
        left = 0;
        right = graph->nodeCount() - 1;
        cursor = 0;
        movingRight = true;
        swappedInScan = false;
        focusA = 0;
        focusB = graph->nodeCount() > 1 ? 1 : -1;
        if (right <= 0) { settleAll(); finished = true; }
    }

    bool advance() override {
        if (finished || left >= right) { finished = true; return false; }

        focusA = cursor;
        focusB = cursor + 1;
        if (valueAt(cursor) > valueAt(cursor + 1)) {
            swapValues(cursor, cursor + 1);
            swappedInScan = true;
        }

        cursor += movingRight ? 1 : -1;
        // 端まで来たらそこが確定し、向きが変わる
        bool atEnd = movingRight ? (cursor >= right) : (cursor < left);
        if (atEnd && turnAround()) {
            settleAll();
            finished = true;
        }
        return true;
    }

public:
    ShakerSortVisualizer() {
        setValuesFrom("5 2 9 1 7 3 8 4");
    }

    emscripten::val getState(emscripten::val params) override {
        emscripten::val state = ArrayVisualizer::getState(params);
        // 向きは動きを見れば分かるが、止めた1枚からは分からない
        state.set("movingRight", movingRight);
        return state;
    }
};
