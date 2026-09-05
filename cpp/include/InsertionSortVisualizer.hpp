#pragma once
#include "ArrayVisualizer.hpp"
#include "GraphColors.hpp"

// 挿入ソートのビジュアライザ。
//
// 左側は並んだ状態を保ち、次の値を**いったん取り出してから**、並んでいる中の
// 入るべき場所へ差し込む。1周は3種類の手でできている。
//
//   取り出す … その値を持ち上げる。持ち上げた場所が空く
//   ずらす   … 空いた場所の左隣が大きければ、それを右へずらして空きを左へ移す
//   差し込む … 左隣が大きくなければ、そこが入る場所。空きに置く
//
// 隣どうしを繰り返し入れ替える書き方でも結果は同じだが、それだとバブルソートと
// 同じ動きに見えてしまう。**取り出して差し込む**のがこのソートの形なので、
// 持ち上げた値を配列の外に出した形にしてある。
//
// 空いた場所には持ち上げた値をそのまま残してある。配列の中身は常に入力の
// 並べ替えになっていて、描画側がその1マスを空に見せている。
class InsertionSortVisualizer : public ArrayVisualizer {
private:
    int next = 1;       // これから取り出す値の位置。ここより左は並んでいる
    int hole = -1;      // 取り出した値が空けている場所。-1 なら取り出していない
    int held = -1;      // 取り出した値
    int droppedAt = -1; // この手で差し込んだ場所。-1 なら差し込んでいない

protected:
    void resetAlgorithm() override {
        next = 1;
        hole = -1;
        held = -1;
        droppedAt = -1;
        focusA = focusB = -1;
        markSettled(0, 0); // 1つだけなら並んでいる
        if (graph->nodeCount() <= 1) { settleAll(); finished = true; }
    }

    // 空いているマスと、直前に差し込んだ場所を足して塗る
    void syncVisuals() override {
        ArrayVisualizer::syncVisuals();
        if (!graph || finished) return;
        if (hole >= 0) graph->setNodeColor(hole, NODE_VISITING);
        if (droppedAt >= 0) graph->setNodeColor(droppedAt, NODE_PATH);
    }

    bool advance() override {
        if (finished) return false;
        droppedAt = -1;

        // 取り出す
        if (hole < 0) {
            if (next >= graph->nodeCount()) { settleAll(); finished = true; return false; }
            hole = next;
            held = valueAt(next);
            focusA = focusB = -1; // 空いたマスは syncVisuals が塗る
            return true;
        }

        // 左隣の方が大きいので、それを右へずらして空きを左へ移す。
        // 空きには持ち上げた値が残っているので、入れ替えがそのままずらしになる
        if (hole > 0 && valueAt(hole - 1) > held) {
            focusA = hole;  // ずらした値の行き先
            focusB = -1;
            swapValues(hole, hole - 1);
            hole--;
            return true;
        }

        // 左隣以上なので、ここが入る場所
        droppedAt = hole;
        focusA = focusB = -1; // 差し込んだ場所は syncVisuals が塗る
        hole = -1;
        held = -1;
        next++;
        markSettled(0, next - 1);
        if (next >= graph->nodeCount()) finished = true;
        return true;
    }

public:
    InsertionSortVisualizer() {
        setValuesFrom("5 2 9 1 7 3 8 4");
    }

    emscripten::val getState(emscripten::val params) override {
        emscripten::val state = ArrayVisualizer::getState(params);
        // 持ち上げている値と、その値が空けている場所。描画側が配列の外に描く
        state.set("heldValue", held);
        state.set("holeIndex", hole);
        // この手で差し込んだ場所。描画側が上から落ちる動きを見せる
        state.set("droppedAt", droppedAt);
        // これから取り出す値の位置
        state.set("insertingAt", finished ? -1 : next);
        return state;
    }
};
