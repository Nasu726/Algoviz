#pragma once
#include "GraphVisualizer.hpp"
#include "TreeLayout.hpp"
#include "GraphColors.hpp"
#include <algorithm>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

// 二分ヒープの構築のビジュアライザ。
//
// 1ステップは「親と一度比べて、必要なら入れ替える」。二分探索木の
// 「1回比べて1つ降りる」と対になる粒度で、根から降りる / 末尾から上がるの
// 対比が見える。
//
// 節点のインデックスがそのままヒープの添字。i の親は (i-1)/2、子は 2i+1 と 2i+2。
// 形は完全二分木で決まるので、入れ替えで動くのは値だけ。辺も座標も動かない。
// 教科書の図と同じ見え方になる。
class HeapVisualizer : public GraphVisualizer {
public:
    static constexpr int MAX_VALUES = MAX_NODES;
    static constexpr int MAX_VALUE  = 99;

private:
    std::vector<int> values;

    // 大きい値が上か。どちらも正しいヒープなので、分類では決まらない。
    bool maxHeap = true;

    struct RunState {
        int pending  = 0;  // これから挿入する値が values の何番目か
        int cursor   = -1; // 今上げている位置。-1 なら次の値を置く前
        int lastSwap = -1; // 直前に入れ替えた相手。色付けに使う
        bool finished = false;
    };

    RunState st;

    // 進めた手数。stepBack は「最初から1手少なく流し直す」で戻すので、
    // 状態のスナップショットは持たない (BstVisualizer と同じ)。
    int stepCount = 0;
    bool replaying = false;

    static int parentOf(int i) { return (i - 1) / 2; }

    float nodeValue(int i) const {
        return graph->nodeData[(std::size_t)i * GraphData::NODE_STRIDE + 2];
    }

    void setNodeValue(int i, float v) {
        graph->nodeData[(std::size_t)i * GraphData::NODE_STRIDE + 2] = v;
    }

    void relayout() {
        if (!replaying) rebuildLayout();
    }

    // 末尾に節点を足す。親との辺は、辺の3列目で左右を指定する。
    // 指定しないと辺の追加順で左右が決まってしまう。
    int pushBack(int value) {
        int idx = graph->nodeCount();
        if (idx == 0) {
            graph->setNode(idx, 0.0f, 0.0f, (float)value, 0);
            graph->startNodeIndex = idx;
            return idx;
        }
        int parent = parentOf(idx);
        std::size_t p = (std::size_t)parent * GraphData::NODE_STRIDE;
        graph->setNode(idx, graph->nodeData[p], graph->nodeData[p + 1], (float)value, 0);
        graph->addEdge((float)parent, (float)idx, (idx % 2 == 1) ? 0.0f : 1.0f, 0);
        return idx;
    }

    // 親子の順序が崩れているか
    bool outOfOrder(int child) const {
        if (child <= 0) return false;
        float c = nodeValue(child), p = nodeValue(parentOf(child));
        return maxHeap ? (c > p) : (c < p);
    }

    int findEdge(int from, int to) const {
        for (int i = 0; i < graph->edgeCount(); i++) {
            if (graph->edgeFrom(i) == from && graph->edgeTo(i) == to) return i;
        }
        return -1;
    }

    // 色は毎回状態から作り直す。差分で塗ると戻したときに前の色が残る。
    void syncVisuals() {
        if (!graph) return;
        graph->resetColors();

        if (st.cursor > 0) {
            int parent = parentOf(st.cursor);
            graph->setNodeColor(parent, NODE_FRONTIER);
            graph->setEdgeColor(findEdge(parent, st.cursor), EDGE_ACTIVE);
        }
        if (st.lastSwap >= 0) graph->setNodeColor(st.lastSwap, NODE_PATH);
        if (st.cursor >= 0) graph->setNodeColor(st.cursor, NODE_VISITING);
    }

    void clearHeap() {
        graph = std::make_unique<GraphData>(MAX_VALUES, MAX_VALUES);
        graph->startNodeIndex = -1;
    }

    void resetRun() {
        clearHeap();
        st = RunState{};
        stepCount = 0;
        relayout();
        syncVisuals();
    }

    void setValuesFrom(const std::string& text) {
        values.clear();
        std::istringstream iss(text);
        int v;
        while (iss >> v && (int)values.size() < MAX_VALUES) values.push_back(v);
        resetRun();
    }

    // 重複しない値を選ぶ。同じ値が並ぶと、入れ替えるかどうかの判断が
    // 目で追いにくくなる。
    void generateValues(int count) {
        count = std::clamp(count, 1, MAX_VALUES);
        std::vector<int> pool(MAX_VALUE);
        std::iota(pool.begin(), pool.end(), 1);
        std::shuffle(pool.begin(), pool.end(), rng);
        values.assign(pool.begin(), pool.begin() + count);
        resetRun();
    }

protected:
    const char* labelMode() const override { return "value"; }

    bool handleCommand(const std::string& source, const std::string& input) override {
        if (source == "setValues") { setValuesFrom(input); return true; }
        if (source == "resetRun")  { resetRun(); return true; }
        if (source == "setMaxHeap") {
            int flag = 1;
            std::istringstream iss(input);
            iss >> flag;
            maxHeap = (flag != 0);
            resetRun(); // 向きが変われば出来上がる木も変わる
            return true;
        }
        if (source == "genRandom") {
            int count = 7;
            std::istringstream iss(input);
            iss >> count;
            generateValues(count);
            return true;
        }
        return false;
    }

public:
    HeapVisualizer() {
        // 基底のコンストラクタが一般グラフを作っているので、木用に置き換える
        layout = std::make_unique<TreeLayout>();
        weighted = false;
        hasNodeWeights = false;
        generatedDirected = false; // 木に矢印は要らない
        skipExtension = false;     // 枝が伸びる様子を見せたいので収束を飛ばさない
        setValuesFrom("50 30 70 20 40 60 80");
    }

    bool step() override {
        if (st.finished) return false;

        // 上げ終わっている。次の値を末尾に置く
        if (st.cursor < 0) {
            if (st.pending >= (int)values.size()) {
                st.finished = true;
                st.lastSwap = -1;
                syncVisuals();
                return false;
            }
            stepCount++;
            int idx = pushBack(values[st.pending]);
            st.pending++;
            st.cursor = idx;
            st.lastSwap = -1;
            relayout();
            syncVisuals();
            return true;
        }

        stepCount++;

        // 根まで上がったら、そこで終わり
        if (st.cursor == 0 || !outOfOrder(st.cursor)) {
            st.cursor = -1;
            st.lastSwap = -1;
            syncVisuals();
            return true;
        }

        // 親と入れ替えて1つ上がる。動くのは値だけ
        int parent = parentOf(st.cursor);
        float tmp = nodeValue(parent);
        setNodeValue(parent, nodeValue(st.cursor));
        setNodeValue(st.cursor, tmp);

        st.lastSwap = st.cursor;
        st.cursor = parent;
        syncVisuals();
        return true;
    }

    // 1手少なく最初から流し直す。節点を消す口が無いので、作り直す方が確実。
    void stepBack() override {
        if (stepCount <= 0) return;
        int target = stepCount - 1;

        replaying = true;
        resetRun();
        for (int i = 0; i < target; i++) step();
        replaying = false;

        rebuildLayout();
        syncVisuals();
    }

    emscripten::val getState(emscripten::val params) override {
        emscripten::val state = GraphVisualizer::getState(params);

        emscripten::val vals = emscripten::val::array();
        for (int v : values) vals.call<void>("push", v);
        state.set("values", vals);

        state.set("pending", st.pending);
        state.set("cursor", st.cursor);
        state.set("finished", st.finished);
        state.set("canStepBack", stepCount > 0);
        state.set("insertedCount", graph ? graph->nodeCount() : 0);
        state.set("maxValues", MAX_VALUES);
        state.set("isMaxHeap", maxHeap);
        return state;
    }
};
