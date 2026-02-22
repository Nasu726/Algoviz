#ifndef GRAPH_VISUALIZER_HPP
#define GRAPH_VISUALIZER_HPP

#include "IVisualizer.hpp"
#include "GraphData.hpp"
#include "StressMajorizationLayout.hpp"
#include <emscripten/val.h>
#include <cstdlib>

using namespace emscripten;

class GraphVisualizer : public IVisualizer {
private:
    GraphData* graph;
    StressMajorizationLayout layout;

public:
    GraphVisualizer() {
        // 初期化時に10ノードのテストデータを作る
        int nodeCount = 10;
        int edgeCount = 10;
        graph = new GraphData(nodeCount, edgeCount);
        
        for (int i = 0; i < nodeCount; i++) {
            float x = rand() % 600;
            float y = rand() % 400;
            float colorId = rand() % 2;
            graph->setNode(i, x, y, 0, colorId);
        }
        for (int i = 0; i < nodeCount; i++) {
            float from = i;
            float to = (i + 1) % nodeCount;
            graph->addEdge(from, to, 0, 0);
        }
        layout.init(graph);
    }

    ~GraphVisualizer() {
        delete graph;
    }

    void load(const std::string& source, const std::string& input) override {
        // 将来、JSから実際のグラフデータを受け取った時も、
        // データをセットし終わった後に layout.init(graph) を呼ぶようにします。
    }

    bool step() override {
        // 毎フレームのアニメーション処理
        // JSから engine.step() を呼ぶとこれが実行される！
        
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