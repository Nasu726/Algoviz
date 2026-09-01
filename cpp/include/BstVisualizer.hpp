#pragma once
#include "GraphVisualizer.hpp"
#include "TreeLayout.hpp"
#include "GraphColors.hpp"
#include <algorithm>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

// 二分探索木の構築のビジュアライザ。
//
// 1ステップは「1回比べて1つ降りる」。根から降りていく様子が見えないと
// 二分探索木を見る意味がないので、1つの値の挿入を1手にはしない。
// BFS / DFS で「1ステップは辺を1本調べる」にしたのと同じ粒度。
//
// 節点の値は GraphData の3列目 (weight 欄) に入れる。labelMode が value なので
// それがそのまま表示名になる。
class BstVisualizer : public GraphVisualizer {
public:
    // 挿入する値の個数の上限。頂点数の上限と揃える。
    static constexpr int MAX_VALUES = MAX_NODES;
    static constexpr int MAX_VALUE  = 99;

private:
    std::vector<int> values;

    // 木の形。節点のインデックスで持つ。-1 は子が無い。
    std::vector<int> leftChild, rightChild;

    struct RunState {
        int pending = 0;       // これから挿入する値が values の何番目か
        int cursor  = -1;      // 今比べている節点。-1 なら次の値の挿入前
        int lastAttached = -1; // 直前に繋いだ節点
        std::vector<int> pathNodes; // 根から降りてきた道
        std::vector<int> pathEdges;
        bool finished  = false;
        bool duplicate = false; // 直前の値が既にあったので捨てた
    };

    RunState st;

    // 進めた手数。stepBack は「最初から1手少なく流し直す」で戻すので、
    // 状態のスナップショットは持たない。
    //
    // GraphData に節点を消す口が無いため、戻すには作り直すのがいちばん確実で短い。
    // 値は 50 個までなので、流し直しの費用は問題にならない。
    int stepCount = 0;
    bool replaying = false;

    float nodeValue(int i) const {
        return graph->nodeData[(std::size_t)i * GraphData::NODE_STRIDE + 2];
    }

    void relayout() {
        if (!replaying) rebuildLayout();
    }

    int addNode(int value, float x, float y) {
        int idx = graph->nodeCount();
        graph->setNode(idx, x, y, (float)value, 0);
        leftChild.push_back(-1);
        rightChild.push_back(-1);
        return idx;
    }

    // 新しい節点は親の位置から生やす。ランダムな位置に置くと画面の端から
    // 飛んでくるので、枝が伸びるようには見えない。
    int addChild(int value, int parent, bool goLeft) {
        std::size_t p = (std::size_t)parent * GraphData::NODE_STRIDE;
        int idx = addNode(value, graph->nodeData[p], graph->nodeData[p + 1]);
        // 辺の3列目は子の並び順。挿入の順に辺が増えるので、これを指定しないと
        // 「50 70 30」で 70 が 30 の左に描かれてしまう。
        graph->addEdge((float)parent, (float)idx, goLeft ? 0.0f : 1.0f, 0);
        if (goLeft) leftChild[parent] = idx;
        else        rightChild[parent] = idx;
        return idx;
    }

    // 色は毎回状態から作り直す。差分で塗ると戻したときに前の色が残る。
    void syncVisuals() {
        if (!graph) return;
        graph->resetColors();

        for (int e : st.pathEdges) graph->setEdgeColor(e, EDGE_VISITED);
        if (!st.pathEdges.empty()) graph->setEdgeColor(st.pathEdges.back(), EDGE_ACTIVE);

        for (int n : st.pathNodes) graph->setNodeColor(n, NODE_VISITED);
        if (st.lastAttached >= 0) graph->setNodeColor(st.lastAttached, NODE_PATH);
        if (st.cursor >= 0) graph->setNodeColor(st.cursor, NODE_VISITING);
    }

    void clearTree() {
        graph = std::make_unique<GraphData>(MAX_VALUES, MAX_VALUES);
        graph->startNodeIndex = -1;
        leftChild.clear();
        rightChild.clear();
    }

    void resetRun() {
        clearTree();
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

    // 重複しない値を個数ぶん選ぶ。同じ値は挿入されないので、
    // ランダム生成で偶然重複すると木が思ったより小さくなって紛らわしい。
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
    BstVisualizer() {
        // 基底のコンストラクタが一般グラフを作っているので、木用に置き換える
        layout = std::make_unique<TreeLayout>();
        weighted = false;
        hasNodeWeights = false;
        generatedDirected = false; // 木に矢印は要らない
        skipExtension = false;     // 枝が伸びる様子を見せたいので収束を飛ばさない
        setValuesFrom("50 30 70 20 40 60 80");
    }

    // 1回比べて1つ降りる。降りる先が空いていればそこに繋いで次の値へ。
    bool step() override {
        if (st.finished) return false;
        st.duplicate = false;

        // 値を入れ終わった
        if (st.pending >= (int)values.size()) {
            st.finished = true;
            st.cursor = -1;
            st.lastAttached = -1;
            syncVisuals();
            return false;
        }

        int v = values[st.pending];
        stepCount++;

        // 木が空なら根を作る
        if (graph->nodeCount() == 0) {
            int r = addNode(v, 0.0f, 0.0f);
            graph->startNodeIndex = r;
            st.lastAttached = r;
            st.pending++;
            st.cursor = -1;
            st.pathNodes.clear();
            st.pathEdges.clear();
            relayout();
            syncVisuals();
            return true;
        }

        // 挿入の始まり。根に降りる
        if (st.cursor < 0) {
            st.cursor = graph->startNodeIndex;
            st.lastAttached = -1;
            st.pathNodes.assign(1, st.cursor);
            st.pathEdges.clear();
            syncVisuals();
            return true;
        }

        int cv = (int)nodeValue(st.cursor);

        // 同じ値は挿入しない
        if (v == cv) {
            st.duplicate = true;
            st.pending++;
            st.cursor = -1;
            syncVisuals();
            return true;
        }

        bool goLeft = v < cv;
        int next = goLeft ? leftChild[st.cursor] : rightChild[st.cursor];

        if (next >= 0) {
            // 子がいるので降りる
            st.pathEdges.push_back(findEdge(st.cursor, next));
            st.cursor = next;
            st.pathNodes.push_back(next);
            syncVisuals();
            return true;
        }

        // 空いている側に繋いで、次の値へ
        int parent = st.cursor;
        int added = addChild(v, parent, goLeft);
        st.pathEdges.push_back(graph->edgeCount() - 1);
        st.pathNodes.push_back(added);
        st.lastAttached = added;
        st.pending++;
        st.cursor = -1;
        relayout();
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
        state.set("duplicate", st.duplicate);
        state.set("canStepBack", stepCount > 0);
        state.set("insertedCount", graph ? graph->nodeCount() : 0);
        state.set("maxValues", MAX_VALUES);
        return state;
    }

private:
    int findEdge(int from, int to) const {
        for (int i = 0; i < graph->edgeCount(); i++) {
            if (graph->edgeFrom(i) == from && graph->edgeTo(i) == to) return i;
        }
        return -1;
    }
};
