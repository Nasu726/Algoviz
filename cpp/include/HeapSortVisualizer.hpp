#pragma once
#include "GraphVisualizer.hpp"
#include "TreeLayout.hpp"
#include "GraphColors.hpp"
#include <algorithm>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

// ヒープソートのビジュアライザ。
//
// ヒープの構築ページと同じ木 (添字が節点番号の完全二分木、形は固定で動くのは
// 値だけ)。**添字順 (上から下、左から右) に読んだものが配列。** 最大ヒープで昇順に固定。
//
//   構築   … 値を全部一度に置き、n/2-1 から 0 へ各節点を「下ろす」。
//             大きい方の子と1回比べ、子が大きければ入れ替えて1つ降りる (1手 = 1回比べる)
//   取り出し … 根と末尾 (未確定の最後) を入れ替え、末尾を確定 (1手)。そのあと根を下ろす
//
// 構築ページは「末尾に足して上げる」、こちらは「下ろす」。対になる。
// 確定した節点は木に残る (形が固定なので消せない)。添字順に読むと後ろから昇順に埋まる。
class HeapSortVisualizer : public GraphVisualizer {
public:
    static constexpr int MAX_VALUES = MAX_NODES;
    static constexpr int MAX_VALUE  = 99;

private:
    std::vector<int> values;

    enum Phase { Build, Extract, Done };

    struct RunState {
        Phase phase = Build;
        int heapSize = 0;   // 未確定の数
        int buildAt  = -1;  // Build: 次に下ろす節点 (n/2-1 → 0)
        int cursor   = -1;  // 下ろしている位置。-1 なら次の節点 / 次の取り出し
        int compared = -1;  // 直前に比べた子
        int lastSwap = -1;  // 直前に入れ替えた相手
        bool finished = false;
    };

    RunState st;

    // stepBack は「最初から1手少なく流し直す」(HeapVisualizer と同じ)
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

    // 末尾に節点を足す。辺の3列目で左右を指定する
    void pushBack(int value) {
        int idx = graph->nodeCount();
        if (idx == 0) {
            graph->setNode(idx, 0.0f, 0.0f, (float)value, 0);
            graph->startNodeIndex = idx;
            return;
        }
        int parent = parentOf(idx);
        std::size_t p = (std::size_t)parent * GraphData::NODE_STRIDE;
        graph->setNode(idx, graph->nodeData[p], graph->nodeData[p + 1], (float)value, 0);
        graph->addEdge((float)parent, (float)idx, (idx % 2 == 1) ? 0.0f : 1.0f, 0);
    }

    int findEdge(int from, int to) const {
        for (int i = 0; i < graph->edgeCount(); i++) {
            if (graph->edgeFrom(i) == from && graph->edgeTo(i) == to) return i;
        }
        return -1;
    }

    // 未確定の範囲にある、大きい方の子。無ければ -1
    int biggerChild(int i) const {
        int l = 2 * i + 1, r = l + 1;
        if (l >= st.heapSize) return -1;
        if (r < st.heapSize && nodeValue(r) > nodeValue(l)) return r;
        return l;
    }

    void swapValues(int a, int b) {
        float tmp = nodeValue(a);
        setNodeValue(a, nodeValue(b));
        setNodeValue(b, tmp);
        // 座標も入れ替えておくと、イージングが戻す間に値が移動して見える
        swapNodePositions(a, b);
    }

    // 色は毎回状態から作り直す
    void syncVisuals() {
        if (!graph) return;
        graph->resetColors();
        int n = graph->nodeCount();
        for (int i = st.heapSize; i < n; i++) graph->setNodeColor(i, NODE_VISITED);
        if (st.lastSwap >= 0) graph->setNodeColor(st.lastSwap, NODE_PATH);
        if (st.compared >= 0) {
            graph->setNodeColor(st.compared, NODE_FRONTIER);
            graph->setEdgeColor(findEdge(parentOf(st.compared), st.compared), EDGE_ACTIVE);
        }
        if (st.cursor >= 0) graph->setNodeColor(st.cursor, NODE_VISITING);
    }

    void resetRun() {
        graph = std::make_unique<GraphData>(MAX_VALUES, MAX_VALUES);
        graph->startNodeIndex = -1;
        for (int v : values) pushBack(v);
        st = RunState{};
        st.heapSize = (int)values.size();
        st.buildAt = st.heapSize / 2 - 1;
        st.finished = values.empty();
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

    // 重複しない値を選ぶ。同じ値が並ぶと入れ替えるかどうかの判断が目で追いにくい
    void generateValues(int count) {
        count = std::clamp(count, 1, MAX_VALUES);
        std::vector<int> pool(MAX_VALUE);
        std::iota(pool.begin(), pool.end(), 1);
        std::shuffle(pool.begin(), pool.end(), rng);
        values.assign(pool.begin(), pool.begin() + count);
        resetRun();
    }

    // 1手進める。true なら手が進んだ
    bool advance() {
        if (st.finished) return false;
        st.compared = -1;
        st.lastSwap = -1;

        // 下ろしている途中: 大きい方の子と1回比べる
        if (st.cursor >= 0) {
            int c = biggerChild(st.cursor);
            if (c < 0 || nodeValue(c) <= nodeValue(st.cursor)) {
                st.compared = c;
                st.cursor = -1;
                return true;
            }
            swapValues(st.cursor, c);
            st.compared = c;
            st.lastSwap = st.cursor;
            st.cursor = c;
            return true;
        }

        if (st.phase == Build) {
            if (st.buildAt >= 0) {
                // 次の節点を下ろし始める。比べるのは次の手
                st.cursor = st.buildAt--;
                return true;
            }
            st.phase = Extract;
        }

        // 取り出し: 根と末尾を入れ替えて末尾を確定
        if (st.heapSize <= 1) {
            st.heapSize = 0;
            st.phase = Done;
            st.finished = true;
            return true;
        }
        int last = st.heapSize - 1;
        swapValues(0, last);
        st.lastSwap = last;
        st.heapSize--;
        st.cursor = 0;
        return true;
    }

protected:
    const char* labelMode() const override { return "value"; }

    bool handleCommand(const std::string& source, const std::string& input) override {
        if (source == "setValues") { setValuesFrom(input); return true; }
        if (source == "resetRun")  { resetRun(); return true; }
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
    HeapSortVisualizer() {
        layout = std::make_unique<TreeLayout>();
        weighted = false;
        hasNodeWeights = false;
        generatedDirected = false;
        skipExtension = false;
        setValuesFrom("20 40 30 80 50 70 60");
    }

    bool step() override {
        if (!advance()) { syncVisuals(); return false; }
        stepCount++;
        syncVisuals();
        return true;
    }

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

        state.set("phase", std::string(st.phase == Build ? "build" : st.phase == Extract ? "extract" : "done"));
        state.set("heapSize", st.heapSize);
        state.set("cursor", st.cursor);
        state.set("compared", st.compared);
        state.set("lastSwap", st.lastSwap);
        state.set("finished", st.finished);
        state.set("canStepBack", stepCount > 0);
        state.set("insertedCount", graph ? graph->nodeCount() : 0);
        state.set("maxValues", MAX_VALUES);
        return state;
    }
};
