#ifndef GRAPH_VISUALIZER_HPP
#define GRAPH_VISUALIZER_HPP

#include "IVisualizer.hpp"
#include "GraphData.hpp"
#include <emscripten/val.h>
#include <cstdlib>

using namespace emscripten;

class GraphVisualizer : public IVisualizer {
private:
    GraphData* graph;

public:
    GraphVisualizer() {
        // 初期化時に1万ノードのテストデータを作る
        int nodeCount = 10;
        int edgeCount = 10;
        graph = new GraphData(nodeCount, edgeCount);
        
        for (int i = 0; i < nodeCount; i++) {
            float x = rand() % 600;
            float y = rand() % 400;
            float colorId = rand() % 2;
            graph->setNode(i, x, y, colorId);
        }
        for (int i = 0; i < edgeCount; i++) {
            int from = rand() % nodeCount;
            int to = rand() % nodeCount;
            graph->addEdge(from, to, 0);
        }
    }

    ~GraphVisualizer() {
        delete graph;
    }

    void load(const std::string& source, const std::string& input) override {
        // 今回は使わないが、将来アルゴリズムの初期化（初期配置）に使う
    }

    bool step() override {
        // 毎フレームのアニメーション（揺らす）処理
        // JSから engine.step() を呼ぶとこれが実行される！
        graph->jiggle();
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