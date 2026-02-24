#ifndef GRAPH_VISUALIZER_HPP
#define GRAPH_VISUALIZER_HPP

#include "IVisualizer.hpp"
#include "GraphData.hpp"
#include "StressMajorizationLayout.hpp"
#include <emscripten/val.h>
#include <cstdlib>
#include <ctime>

using namespace emscripten;

class GraphVisualizer : public IVisualizer {
private:
    GraphData* graph;
    StressMajorizationLayout layout;

public:
    GraphVisualizer() {
        srand((unsigned int)time(nullptr));

        // 初期化時に10ノードのテストデータを作る
        int nodeCount = 20;
        int edgeCount = 20;
        graph = new GraphData(nodeCount, edgeCount);
        
        for (int i = 0; i < nodeCount; i++) {
            float x = rand() % 600 + 100;
            float y = rand() % 400 + 100;
            float colorId = rand() % 2;
            graph->setNode(i, x, y, 0, colorId);
        }
        if (false) {
            for (int i=0; i< nodeCount; i++){
                for (int j=i+1; j<nodeCount;  j++){
                    graph->addEdge(i, j, 0, 0);
                }
            }
        } else {
            for (int i = 0; i < edgeCount; i++) {
                float from = rand() % nodeCount;
                float to = rand() % nodeCount;
                graph->addEdge(from, to, 0, 0);
            }
        }
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

        // 設定が変更されたら、再び計算を走らせるためにフラグをリセットし、少し揺らす
        layout.is_stable = false;
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