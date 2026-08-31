#pragma once
#include "GraphVisualizer.hpp"
#include <set>
#include <sstream>
#include <string>

// オートマトン表示。
//
// オートマトンは有向グラフしか存在しないので、一般グラフのクラスを継承して
// 「常に有向」「初期状態」「受理状態」だけを足す。
// 初期状態と受理状態はアルゴリズムの論理なので JS 側ではなく C++ 側が持つ。
class AutomatonVisualizer : public GraphVisualizer {
private:
    std::set<int> acceptingStates;

protected:
    bool forceDirected() const override { return true; }

    // グラフが差し替わったら、範囲外になった状態指定を捨てる
    void onGraphChanged() override {
        int n = graph ? graph->nodeCount() : 0;
        if (graph && (graph->startNodeIndex < 0 || graph->startNodeIndex >= n)) {
            graph->startNodeIndex = -1;
        }
        for (auto it = acceptingStates.begin(); it != acceptingStates.end(); ) {
            if (*it < 0 || *it >= n) it = acceptingStates.erase(it);
            else ++it;
        }
    }

    bool handleCommand(const std::string& source, const std::string& input) override {
        if (source == "setStartNode") {
            int idx = -1;
            std::istringstream iss(input);
            iss >> idx;
            if (graph) {
                graph->startNodeIndex =
                    (idx >= 0 && idx < graph->nodeCount()) ? idx : -1;
            }
            return true;
        }

        if (source == "setAccepting") {
            // "1, 2, 3" のようなカンマ区切りも受け付ける
            std::string normalized = input;
            for (char& c : normalized) if (c == ',') c = ' ';

            acceptingStates.clear();
            std::istringstream iss(normalized);
            int idx;
            int n = graph ? graph->nodeCount() : 0;
            while (iss >> idx) {
                if (idx >= 0 && idx < n) acceptingStates.insert(idx);
            }
            return true;
        }

        return false;
    }

public:
    emscripten::val getState(emscripten::val params) override {
        emscripten::val state = GraphVisualizer::getState(params);
        state.set("isAutomaton", true);

        emscripten::val accepting = emscripten::val::array();
        for (int idx : acceptingStates) accepting.call<void>("push", idx);
        state.set("acceptingStates", accepting);

        return state;
    }
};
