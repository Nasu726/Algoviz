#pragma once
#include "ArrayVisualizer.hpp"
#include "GraphColors.hpp"
#include <algorithm>
#include <sstream>
#include <string>

// 配列から値を探すものの基底。線形探索と二分探索がこれを継承する。
//
// ソートと違って**終わり方が2通りある**。見つけて終わるか、見る場所が
// 無くなって終わるか。どちらで終わったかは foundAt で分かる。
//
// settled は「もう見ない範囲」として使う。ソートの「位置が確定した」とは
// 意味が違うので、凡例の言葉は描画側で分けている。
class SearchVisualizer : public ArrayVisualizer {
protected:
    int target = 0;   // 探す値
    int foundAt = -1; // 見つけた位置。-1 なら見つかっていない

    // 探す値も含めて作り直す。値を変えたら探し直しになる
    void resetAlgorithm() override { foundAt = -1; }

    // 見つけた値は、見終わった灰色とは別に見せる
    void syncVisuals() override {
        ArrayVisualizer::syncVisuals();
        if (!graph) return;
        if (foundAt >= 0) graph->setNodeColor(foundAt, NODE_PATH);
    }

    bool handleCommand(const std::string& source, const std::string& input) override {
        if (source == "setTarget") {
            std::istringstream iss(input);
            int v;
            if (iss >> v) target = v;
            resetRun();
            return true;
        }
        return ArrayVisualizer::handleCommand(source, input);
    }

    // 入力が昇順に並んでいるか。二分探索が前提にしているもの
    bool isSorted() const {
        for (int i = 1; i < rowSize() && i < graph->nodeCount(); i++) {
            if (valueAt(i - 1) > valueAt(i)) return false;
        }
        return true;
    }

public:
    emscripten::val getState(emscripten::val params) override {
        emscripten::val state = ArrayVisualizer::getState(params);
        state.set("target", target);
        state.set("foundAt", foundAt);
        state.set("sorted", isSorted());
        return state;
    }
};
