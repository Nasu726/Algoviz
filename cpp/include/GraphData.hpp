#pragma once
#include <vector>
#include <emscripten/val.h>

// 視覚データの管理クラス（各アルゴリズムクラスのメンバとして持たせる）
//
// JS 側とは typed_memory_view でゼロコピー共有するため、
// レイアウトは float の平坦な配列に固定されている。
class GraphData {
public:
    std::vector<float> nodeData; // [x0, y0, weight0, color0, x1, y1, ...]
    std::vector<float> edgeData; // [from0, to0, weight0, color0, from1, to1, ...]
    static constexpr int NODE_STRIDE = 4; // 1ノードあたりのデータ数
    static constexpr int EDGE_STRIDE = 4; // 1エッジあたりのデータ数
    int startNodeIndex = -1;              // オートマトンの初期状態のインデックス

    // 節点ごとの半幅。B木のように1つの節点が複数の値を持つものは、
    // 半径 20 の円1つでは表せないので節点ごとに幅を持たせる。
    //
    // nodeData の stride は増やさない。ゼロコピー共有の並びを崩さないよう、
    // 別の配列にしてある。
    static constexpr float DEFAULT_HALF_WIDTH = 20.0f;
    std::vector<float> nodeHalfWidth;

    GraphData(int maxNodes, int maxEdges) {
        nodeData.reserve(maxNodes * NODE_STRIDE);
        edgeData.reserve(maxEdges * EDGE_STRIDE);
    }

    int nodeCount() const { return (int)nodeData.size() / NODE_STRIDE; }
    int edgeCount() const { return (int)edgeData.size() / EDGE_STRIDE; }

    // 節点の半幅。指定していなければ既定値
    float halfWidthOf(int index) const {
        if (index < 0 || index >= (int)nodeHalfWidth.size()) return DEFAULT_HALF_WIDTH;
        return nodeHalfWidth[index];
    }

    void setHalfWidth(int index, float halfWidth) {
        if (index < 0) return;
        if ((int)nodeHalfWidth.size() <= index) nodeHalfWidth.resize(index + 1, DEFAULT_HALF_WIDTH);
        nodeHalfWidth[index] = halfWidth;
    }

    void setNode(int index, float x, float y, float weight, float colorId) {
        std::size_t offset = (std::size_t)index * NODE_STRIDE;
        if (offset + NODE_STRIDE > nodeData.size()) nodeData.resize(offset + NODE_STRIDE, 0.0f);
        if ((int)nodeHalfWidth.size() < nodeCountFor(offset)) {
            nodeHalfWidth.resize(nodeCountFor(offset), DEFAULT_HALF_WIDTH);
        }
        nodeData[offset]     = x;
        nodeData[offset + 1] = y;
        nodeData[offset + 2] = weight;
        nodeData[offset + 3] = colorId;
    }

    void addEdge(float fromIndex, float toIndex, float weight, float colorId) {
        edgeData.push_back(fromIndex);
        edgeData.push_back(toIndex);
        edgeData.push_back(weight);
        edgeData.push_back(colorId);
    }

    int edgeFrom(int i) const { return (int)edgeData[i * EDGE_STRIDE]; }
    int edgeTo(int i)   const { return (int)edgeData[i * EDGE_STRIDE + 1]; }

    // 色は「意味」を表す整数 (GraphColors.hpp)。実際の配色は JS 側が決める。
    void setNodeColor(int i, int colorId) {
        if (i >= 0 && i < nodeCount()) nodeData[i * NODE_STRIDE + 3] = (float)colorId;
    }
    void setEdgeColor(int i, int colorId) {
        if (i >= 0 && i < edgeCount()) edgeData[i * EDGE_STRIDE + 3] = (float)colorId;
    }
    int nodeColor(int i) const { return (int)nodeData[i * NODE_STRIDE + 3]; }
    int edgeColor(int i) const { return (int)edgeData[i * EDGE_STRIDE + 3]; }

private:
    static int nodeCountFor(std::size_t offsetEnd) {
        return (int)(offsetEnd / NODE_STRIDE) + 1;
    }

public:
    void resetColors() {
        for (int i = 0; i < nodeCount(); i++) nodeData[i * NODE_STRIDE + 3] = 0.0f;
        for (int i = 0; i < edgeCount(); i++) edgeData[i * EDGE_STRIDE + 3] = 0.0f;
    }

    // JS側にゼロコピーでメモリを公開する
    emscripten::val getNodeHalfWidthView() {
        // 節点の数だけ必ず並んでいるようにしてから渡す
        if ((int)nodeHalfWidth.size() < nodeCount()) {
            nodeHalfWidth.resize(nodeCount(), DEFAULT_HALF_WIDTH);
        }
        return emscripten::val(emscripten::typed_memory_view(nodeHalfWidth.size(),
                                                             nodeHalfWidth.data()));
    }

    emscripten::val getNodeView() {
        return emscripten::val(emscripten::typed_memory_view(nodeData.size(), nodeData.data()));
    }
    emscripten::val getEdgeView() {
        return emscripten::val(emscripten::typed_memory_view(edgeData.size(), edgeData.data()));
    }
};
