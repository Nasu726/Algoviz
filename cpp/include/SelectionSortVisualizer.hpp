#pragma once
#include "ArrayVisualizer.hpp"

// 選択ソートのビジュアライザ。
//
// 未確定の範囲から最小の値を探し、見つけたら先頭と入れ替える。
// 左から順に位置が確定していく。
//
// **バブルソートと違い、比べるのと入れ替えるのを別の手にしてある。**
// 入れ替えは1周につき1回しか起きないので、分けても手数はほとんど増えない。
// むしろ「探すのに何回も比べるのに、入れ替えは1回だけ」がこのソートの見どころで、
// 同じ手にすると見えなくなる。
//
// 最小の値が既に先頭にあっても入れ替えの手は踏むし、**残りが1つになった最後の周も
// 飛ばさない。** どの位置も同じ手順で決まる形にしてある。
//
// 確定した範囲を走査から外すのは速さの工夫ではない。そこを見に行くと既に置いた
// 小さい値を拾い直してしまうので、外せない。
class SelectionSortVisualizer : public ArrayVisualizer {
private:
    int head = 0;      // ここから右が未確定。ここに最小の値を入れる
    int cursor = 0;    // 今見ている位置
    int minIndex = 0;  // 今のところ最小の値がある位置
    bool swapping = false; // 次の手で入れ替える

    // 1周の始め。見る先が無い最後の周は、入れ替えの手だけになる
    void beginRound() {
        minIndex = head;
        cursor = head + 1;
        swapping = cursor >= graph->nodeCount();
    }

protected:
    void resetAlgorithm() override {
        head = 0;
        focusA = focusB = -1;
        beginRound();
        if (graph->nodeCount() <= 0) finished = true;
    }

    // 今のところ最小の値は、比べている値とは別の色で示す
    void syncVisuals() override {
        ArrayVisualizer::syncVisuals();
        if (!finished && !swapping) graph->setNodeColor(minIndex, NODE_FRONTIER);
    }

    bool advance() override {
        if (finished) return false;

        // 探し終えたので、見つけた最小の値を先頭へ持ってくる
        if (swapping) {
            focusA = head;
            focusB = minIndex;
            // 既に先頭にあっても入れ替えの手は踏む。教科書どおりの形にしておく
            swapValues(head, minIndex);
            markSettled(head, head);
            head++;

            if (head >= graph->nodeCount()) finished = true;
            else beginRound();
            return true;
        }

        // 未確定の範囲を1つずつ見て、最小を覚えておく
        focusA = cursor;
        focusB = -1;
        if (valueAt(cursor) < valueAt(minIndex)) minIndex = cursor;
        cursor++;
        if (cursor >= graph->nodeCount()) swapping = true;
        return true;
    }

public:
    SelectionSortVisualizer() {
        setValuesFrom("5 2 9 1 7 3 8 4");
    }

    emscripten::val getState(emscripten::val params) override {
        emscripten::val state = ArrayVisualizer::getState(params);
        state.set("minIndex", finished ? -1 : minIndex);
        // 次の手で入れ替えるか。探している最中とで見せる言葉が変わる
        state.set("swapping", swapping);
        return state;
    }
};
