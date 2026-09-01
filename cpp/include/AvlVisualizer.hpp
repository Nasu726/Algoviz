#pragma once
#include "GraphVisualizer.hpp"
#include "TreeLayout.hpp"
#include "GraphColors.hpp"
#include <algorithm>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

// AVL 木の構築のビジュアライザ。
//
// 二分探索木と同じように値を1つずつ挿入し、挿入した道を根へ戻りながら
// 高さの偏りを直す。1ステップは
//   降りるとき … 「1回比べて1つ降りる」(二分探索木と同じ)
//   戻るとき   … 「1つの節点の偏りを見て、必要なら回す」
//
// 丸の中は値、脇の数字は偏り (左の高さ - 右の高さ)。偏りが 2 か -2 に
// なった節点で回転が起きる。
//
// 回転は木の形そのものを変えるので、辺を張り直して配置を計算し直す。
// 目標の座標が変わるので、既存のイージングが回る様子を見せてくれる。
// (値だけが動くヒープと違い、swapNodePositions は要らない)
class AvlVisualizer : public GraphVisualizer {
public:
    static constexpr int MAX_VALUES = MAX_NODES;
    static constexpr int MAX_VALUE  = 99;

private:
    std::vector<int> values;

    // 木の形。節点のインデックスで持つ。-1 は子が無い。
    std::vector<int> leftChild, rightChild, height;
    std::vector<int> key;
    int root = -1;

    struct RunState {
        int pending = 0;   // これから挿入する値が values の何番目か
        int cursor  = -1;  // 降りている途中の節点。-1 なら次の値の前
        std::vector<int> path; // 根から辿った道。戻るときに使う
        int checking = -1; // 今偏りを見ている節点
        int lastRotated = -1;
        int rotation = 0;  // 直前の回転。0 無し / 1 右 / 2 左 / 3 左右 / 4 右左
        bool duplicate = false;
        bool finished = false;
    };

    RunState st;

    int stepCount = 0;
    bool replaying = false;

    void relayout() {
        if (!replaying) rebuildLayout();
    }

    int heightOf(int n) const { return n < 0 ? 0 : height[n]; }
    int balanceOf(int n) const {
        return n < 0 ? 0 : heightOf(leftChild[n]) - heightOf(rightChild[n]);
    }
    void updateHeight(int n) {
        height[n] = 1 + std::max(heightOf(leftChild[n]), heightOf(rightChild[n]));
    }

    int addNode(int value) {
        int idx = graph->nodeCount();
        graph->setNode(idx, 0.0f, 0.0f, 0, 0);
        key.push_back(value);
        leftChild.push_back(-1);
        rightChild.push_back(-1);
        height.push_back(1);
        return idx;
    }

    // 回転で親子が入れ替わるので、辺は毎回張り直す。
    // 節点は 50 個までなので、作り直す方が張り替えより短くて確実。
    void rebuildEdges() {
        graph->edgeData.clear();
        for (int u = 0; u < graph->nodeCount(); u++) {
            // 辺の3列目は子の並び順。0 が左、1 が右
            if (leftChild[u] >= 0)  graph->addEdge((float)u, (float)leftChild[u], 0.0f, 0);
            if (rightChild[u] >= 0) graph->addEdge((float)u, (float)rightChild[u], 1.0f, 0);
        }
        // 偏りを節点の脇に出す
        for (int u = 0; u < graph->nodeCount(); u++) {
            graph->nodeData[(std::size_t)u * GraphData::NODE_STRIDE + 2] = (float)balanceOf(u);
        }
        graph->startNodeIndex = root;
    }

    int rotateRight(int y) {
        int x = leftChild[y];
        leftChild[y] = rightChild[x];
        rightChild[x] = y;
        updateHeight(y);
        updateHeight(x);
        return x;
    }

    int rotateLeft(int x) {
        int y = rightChild[x];
        rightChild[x] = leftChild[y];
        leftChild[y] = x;
        updateHeight(x);
        updateHeight(y);
        return y;
    }

    // 偏りが 2 か -2 なら回して直し、その部分木の新しい根を返す。
    // 直す必要が無ければ受け取った節点をそのまま返す。
    int rebalanceOnce(int n, int& kind) {
        kind = 0;
        int b = balanceOf(n);
        if (b > 1) {
            if (balanceOf(leftChild[n]) < 0) { // 左の子が右に偏る = 左右
                leftChild[n] = rotateLeft(leftChild[n]);
                kind = 3;
            } else {
                kind = 1;
            }
            return rotateRight(n);
        }
        if (b < -1) {
            if (balanceOf(rightChild[n]) > 0) { // 右の子が左に偏る = 右左
                rightChild[n] = rotateRight(rightChild[n]);
                kind = 4;
            } else {
                kind = 2;
            }
            return rotateLeft(n);
        }
        return n;
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

        for (std::size_t i = 1; i < st.path.size(); i++) {
            graph->setNodeColor(st.path[i], NODE_VISITED);
            graph->setEdgeColor(findEdge(st.path[i - 1], st.path[i]), EDGE_VISITED);
        }
        if (!st.path.empty()) graph->setNodeColor(st.path.front(), NODE_VISITED);

        // 見ているだけの節点より、回した節点の方を優先して見せる
        if (st.checking >= 0) graph->setNodeColor(st.checking, NODE_FRONTIER);
        if (st.lastRotated >= 0) {
            graph->setNodeColor(st.lastRotated, NODE_PATH);
            if (leftChild[st.lastRotated] >= 0)
                graph->setEdgeColor(findEdge(st.lastRotated, leftChild[st.lastRotated]), EDGE_ACTIVE);
            if (rightChild[st.lastRotated] >= 0)
                graph->setEdgeColor(findEdge(st.lastRotated, rightChild[st.lastRotated]), EDGE_ACTIVE);
        }
        if (st.cursor >= 0)   graph->setNodeColor(st.cursor, NODE_VISITING);
    }

    void clearTree() {
        graph = std::make_unique<GraphData>(MAX_VALUES, MAX_VALUES * 2);
        graph->startNodeIndex = -1;
        key.clear();
        leftChild.clear();
        rightChild.clear();
        height.clear();
        root = -1;
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

    void generateValues(int count) {
        count = std::clamp(count, 1, MAX_VALUES);
        std::vector<int> pool(MAX_VALUE);
        std::iota(pool.begin(), pool.end(), 1);
        std::shuffle(pool.begin(), pool.end(), rng);
        values.assign(pool.begin(), pool.begin() + count);
        resetRun();
    }

protected:
    const char* labelMode() const override { return "text"; }

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
    AvlVisualizer() {
        // 基底のコンストラクタが一般グラフを作っているので、木用に置き換える
        layout = std::make_unique<TreeLayout>();
        weighted = false;
        hasNodeWeights = true;     // 偏りを節点の脇に出す
        generatedDirected = false; // 木に矢印は要らない
        skipExtension = false;     // 回る様子を見せたいので収束を飛ばさない
        setValuesFrom("10 20 30 40 50 25");
    }

    bool step() override {
        if (st.finished) return false;
        st.duplicate = false;

        // --- 挿入した道を根へ戻りながら偏りを直す ---
        if (!st.path.empty() && st.cursor < 0) {
            stepCount++;
            int u = st.path.back();
            st.path.pop_back();

            updateHeight(u);
            int kind = 0;
            int nu = rebalanceOnce(u, kind);

            // 回った結果を親に繋ぎ直す
            if (nu != u) {
                if (st.path.empty()) root = nu;
                else if (leftChild[st.path.back()] == u) leftChild[st.path.back()] = nu;
                else rightChild[st.path.back()] = nu;
            }

            st.checking = nu;
            st.rotation = kind;
            st.lastRotated = (kind != 0) ? nu : -1;

            rebuildEdges();
            relayout();
            syncVisuals();
            return true;
        }

        // --- 次の値の挿入を始める ---
        if (st.cursor < 0) {
            if (st.pending >= (int)values.size()) {
                st.finished = true;
                st.checking = st.lastRotated = -1;
                st.rotation = 0;
                syncVisuals();
                return false;
            }
            stepCount++;
            st.checking = st.lastRotated = -1;
            st.rotation = 0;

            if (root < 0) {
                root = addNode(values[st.pending]);
                st.pending++;
                st.cursor = -1;
                rebuildEdges();
                relayout();
                syncVisuals();
                return true;
            }

            st.cursor = root;
            st.path.assign(1, root);
            syncVisuals();
            return true;
        }

        stepCount++;
        int v = values[st.pending];
        int cv = key[st.cursor];

        // 同じ値は挿入しない
        if (v == cv) {
            st.duplicate = true;
            st.pending++;
            st.cursor = -1;
            st.path.clear();
            syncVisuals();
            return true;
        }

        bool goLeft = v < cv;
        int next = goLeft ? leftChild[st.cursor] : rightChild[st.cursor];

        if (next >= 0) {
            st.cursor = next;
            st.path.push_back(next);
            syncVisuals();
            return true;
        }

        // 空いている側に繋ぐ。ここから戻りながら偏りを直す
        int added = addNode(v);
        if (goLeft) leftChild[st.cursor] = added;
        else        rightChild[st.cursor] = added;

        // 新しい節点は親の位置から生やす
        std::size_t p = (std::size_t)st.cursor * GraphData::NODE_STRIDE;
        std::size_t a = (std::size_t)added * GraphData::NODE_STRIDE;
        graph->nodeData[a]     = graph->nodeData[p];
        graph->nodeData[a + 1] = graph->nodeData[p + 1];

        st.pending++;
        st.cursor = -1;
        st.lastRotated = added;

        rebuildEdges();
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

        // 丸の中に出す値
        emscripten::val ls = emscripten::val::array();
        for (int k : key) ls.call<void>("push", std::to_string(k));
        state.set("nodeLabels", ls);

        state.set("pending", st.pending);
        state.set("cursor", st.cursor);
        state.set("checking", st.checking);
        state.set("rotation", st.rotation);
        state.set("duplicate", st.duplicate);
        state.set("finished", st.finished);
        state.set("canStepBack", stepCount > 0);
        state.set("insertedCount", graph ? graph->nodeCount() : 0);
        state.set("maxValues", MAX_VALUES);
        state.set("treeHeight", heightOf(root));
        return state;
    }
};
