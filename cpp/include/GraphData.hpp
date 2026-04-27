#pragma once
#include <vector>
#include <emscripten/bind.h>
#include <emscripten/val.h>
using namespace emscripten;

// 視覚データの管理クラス（各アルゴリズムクラスのメンバとして持たせる）
class GraphData {
public:
    std::vector<float> nodeData; // [x0, y0, weight0, color0, x1, y1, weight1, color1, ...]
    std::vector<float> edgeData; // [from0, to0, weight0, color0, from1, to1, ...]
    const int NODE_STRIDE = 5;   // 1ノードあたりのデータ数
    const int EDGE_STRIDE = 4;   // 1エッジあたりのデータ数
    int startNodeIndex = -1;     // オートマトンの初期状態のインデックス

    GraphData(int maxNodes, int maxEdges) {
        nodeData.reserve(maxNodes * NODE_STRIDE);
        edgeData.reserve(maxEdges * EDGE_STRIDE);
    }

    void jiggle() {
        for (int i=0; i < nodeData.size(); i += NODE_STRIDE) {
            float dx = ((rand() % 200) - 100) / 100.0f;
            float dy = ((rand() % 200) - 100) / 100.0f;
            nodeData[i] += dx;
            nodeData[i+1] += dy;
        }
    }

    void setNode(int index, float x, float y, float weight, float colorId, float fixed = 0.0f) {
        int offset = index * NODE_STRIDE;
        if (offset + NODE_STRIDE > nodeData.size()) nodeData.resize(offset + NODE_STRIDE, 0.0f);
        nodeData[offset]     = x;
        nodeData[offset + 1] = y;
        nodeData[offset + 2] = weight;
        nodeData[offset + 3] = colorId;
        nodeData[offset + 4] = fixed;
    }

    void addEdge(float fromIndex, float toIndex, float weight, float colorId) {
        edgeData.push_back(fromIndex);
        edgeData.push_back(toIndex);
        edgeData.push_back(weight);
        edgeData.push_back(colorId);
    }

    // JS側にゼロコピーでメモリを公開する魔法
    val getNodeView() { return val(typed_memory_view(nodeData.size(), nodeData.data())); }
    val getEdgeView() { return val(typed_memory_view(edgeData.size(), edgeData.data())); }
};

EMSCRIPTEN_BINDINGS(graph_module) {
    class_<GraphData>("GraphData")
        .constructor<int, int>()
        .function("getNodeView", &GraphData::getNodeView)
        .function("getEdgeView", &GraphData::getEdgeView)
        .function("jiggle", &GraphData::jiggle);
}