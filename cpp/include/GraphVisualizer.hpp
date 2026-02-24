#ifndef GRAPH_VISUALIZER_HPP
#define GRAPH_VISUALIZER_HPP

#include "IVisualizer.hpp"
#include "GraphData.hpp"
#include "StressMajorizationLayout.hpp"
#include <emscripten/val.h>
#include <cstdlib>
#include <ctime>
#include <sstream>

using namespace emscripten;

class GraphVisualizer : public IVisualizer {
private:
    GraphData* graph;
    StressMajorizationLayout layout;

    // ★ グラフを新しく作り直すヘルパー関数
    void generateRandom(int v, int e) {
        delete graph;
        graph = new GraphData(v, e);
        for (int i = 0; i < v; i++) {
            graph->setNode(i, rand() % 600 + 100, rand() % 400 + 100, 0, 0);
        }
        for (int i = 0; i < e; i++) {
            int from = rand() % v;
            int to = rand() % v;
            float weight = rand() % 100; // ランダムな重み
            graph->addEdge(from, to, weight, 0);
        }
    }

    void generateComplete(int v) {
        delete graph;
        int e = v * (v - 1) / 2;
        graph = new GraphData(v, e);
        for (int i = 0; i < v; i++) {
            graph->setNode(i, rand() % 600 + 100, rand() % 400 + 100, 0, 0);
        }
        for (int i = 0; i < v; i++) {
            for (int j = i + 1; j < v; j++) {
                float weight = rand() % 100;
                graph->addEdge(i, j, weight, 0);
            }
        }
    }

public:
    GraphVisualizer() {
        srand((unsigned int)time(nullptr));
        graph = nullptr;
        generateRandom(5, 7);
        layout.init(graph);
    }

    ~GraphVisualizer() {
        delete graph;
    }

    void load(const std::string& source, const std::string& input) override {
        // 将来、JSから実際のグラフデータを受け取った時も、
        // データをセットし終わった後に layout.init(graph) を呼ぶようにします。
        // source 引数を使って向きを設定
        if (source == "horizontal")    layout.preferHorizontal = true;
        else if (source == "vertical") layout.preferHorizontal = false;

        // ★ Reactから送られたコマンドの解析
        if (!input.empty()) {
            std::istringstream iss(input);
            std::string cmd;
            iss >> cmd;
            if (cmd == "random") {
                int v, e;
                iss >> v >> e;
                generateRandom(v, e);
                layout.init(graph);
            } else if (cmd == "complete") {
                int v;
                iss >> v;
                generateComplete(v);
                layout.init(graph);
            }
        }
        
        layout.is_stable = false;
    }

    bool step() override {
        layout.update(graph);
        return true;
    }

    void stepBack() override {
        // グラフの1手戻る処理（後で実装）
    }

    val getState(val params) override {
        // 状態として GraphData のインスタンスをそのままJSに渡す
        val state = val::object();

        state.set("nodes", graph->getNodeView());
        state.set("edges", graph->getEdgeView());

        return state;
    }

    std::string getOutput() override {
        return "";
    }
};

#endif