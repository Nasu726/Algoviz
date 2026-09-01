#pragma once
#include "GraphVisualizer.hpp"
#include "TreeLayout.hpp"
#include "GraphColors.hpp"
#include <algorithm>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

// B木の構築のビジュアライザ。
//
// 1つの節点が最大 m-1 個の値と最大 m 個の子を持つ。半径 20 の円1つでは
// 表せないので、値の数から決めた半幅を GraphData に入れて横長に描かせる。
// TreeLayout もその幅を見て兄弟を離す。
//
// 1ステップは3種類。
//   どの子へ降りるか決める / 葉に入れる / あふれたので分割する
//
// 分割は木の形そのものを変えるので、AVL と同じく辺を張り直して
// rebuildLayout に任せれば、既存のイージングが動きを見せてくれる。
class BTreeVisualizer : public GraphVisualizer {
public:
    // 次数3 では値1個の節点が並ぶので、値の数と同じだけ節点ができうる。
    // 頂点の上限 50 に収まるところで切る。
    static constexpr int MAX_VALUES = 30;
    static constexpr int MAX_VALUE  = 99;
    static constexpr int MIN_ORDER  = 3;
    static constexpr int MAX_ORDER  = 5;
    static constexpr int DEFAULT_ORDER = 4;

private:
    std::vector<int> values;
    int order = DEFAULT_ORDER; // 1つの節点が持てる子の数の上限

    // 木の形。keys は昇順、children は空 (葉) か keys の数 + 1 個。
    std::vector<std::vector<int>> keys;
    std::vector<std::vector<int>> children;
    int root = -1;

    struct RunState {
        int pending = 0;  // これから挿入する値が values の何番目か
        int cursor  = -1; // 降りている途中の節点。-1 なら降りていない
        std::vector<int> route; // 根から今の節点まで。分割で上へ戻るのに使う
        int splitDepth = -1;    // route のこの深さの節点があふれている。-1 なら無い
        int lastTouched = -1;   // 直前に値を入れた / 押し上げた節点
        bool splitting = false; // 直前の手が分割だった
        bool duplicate = false;
        bool finished = false;
    };

    RunState st;

    int stepCount = 0;
    bool replaying = false;

    void relayout() {
        if (!replaying) rebuildLayout();
    }

    int maxKeys() const { return order - 1; }
    bool isLeaf(int n) const { return children[n].empty(); }
    bool overflows(int n) const { return (int)keys[n].size() > maxKeys(); }

    // 値を並べたものが節点の表示名。半幅もここから決める。
    std::string labelOf(int n) const {
        std::string label;
        for (std::size_t i = 0; i < keys[n].size(); i++) {
            if (i) label += " ";
            label += std::to_string(keys[n][i]);
        }
        return label;
    }

    // 新しい節点は種にした節点の位置から生やす。原点から飛んでくるのを避ける。
    int addNode(int seed) {
        int idx = graph->nodeCount();
        float x = 0.0f, y = 0.0f;
        if (seed >= 0) {
            std::size_t s = (std::size_t)seed * GraphData::NODE_STRIDE;
            x = graph->nodeData[s];
            y = graph->nodeData[s + 1];
        }
        graph->setNode(idx, x, y, 0, 0);
        keys.emplace_back();
        children.emplace_back();
        return idx;
    }

    // 分割で親子が入れ替わるので、辺と幅は毎回作り直す。
    // 節点は数十個までなので、張り替えより作り直す方が短くて確実。
    void rebuildEdges() {
        graph->edgeData.clear();
        for (int u = 0; u < graph->nodeCount(); u++) {
            for (std::size_t i = 0; i < children[u].size(); i++) {
                // 辺の3列目は子の並び順。TreeLayout がこの順に左から並べる
                graph->addEdge((float)u, (float)children[u][i], (float)i, 0);
            }
            // 16px 太字の数字がおよそ 9px、空白がおよそ 4.5px。両端に余白を足す
            float half = (float)labelOf(u).size() * 4.5f + 10.0f;
            graph->setHalfWidth(u, std::max(GraphData::DEFAULT_HALF_WIDTH, half));
        }
        graph->startNodeIndex = root;
    }

    // route[depth] の節点を2つに割り、真ん中の値を親へ押し上げる。
    // 親が無ければ (根の分割) 新しい根を作り、木が1段深くなる。
    void splitAt(int depth) {
        int n = st.route[depth];
        int parent = depth > 0 ? st.route[depth - 1] : -1;
        int mid = (int)keys[n].size() / 2;
        int midKey = keys[n][mid];

        int right = addNode(n);
        keys[right].assign(keys[n].begin() + mid + 1, keys[n].end());
        if (!isLeaf(n)) {
            children[right].assign(children[n].begin() + mid + 1, children[n].end());
            children[n].resize(mid + 1);
        }
        keys[n].resize(mid);

        if (parent < 0) {
            int newRoot = addNode(n);
            keys[newRoot].push_back(midKey);
            children[newRoot] = {n, right};
            root = newRoot;
            st.lastTouched = newRoot;
            st.route.clear();
            st.splitDepth = -1;
        } else {
            std::size_t slot = 0;
            while (slot < children[parent].size() && children[parent][slot] != n) slot++;
            keys[parent].insert(keys[parent].begin() + slot, midKey);
            children[parent].insert(children[parent].begin() + slot + 1, right);
            st.lastTouched = parent;
            st.route.resize(depth); // 割った節点から下は、もう辿る道ではない
            st.splitDepth = overflows(parent) ? (int)st.route.size() - 1 : -1;
        }
    }

    // この値の挿入を終える。次の手は次の値から始まる。
    void finishValue() {
        st.pending++;
        st.cursor = -1;
        st.splitDepth = -1;
        st.route.clear();
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

        for (std::size_t i = 0; i < st.route.size(); i++) {
            graph->setNodeColor(st.route[i], NODE_VISITED);
            if (i) graph->setEdgeColor(findEdge(st.route[i - 1], st.route[i]), EDGE_VISITED);
        }

        if (st.lastTouched >= 0) {
            graph->setNodeColor(st.lastTouched, NODE_PATH);
            for (int c : children[st.lastTouched]) {
                graph->setEdgeColor(findEdge(st.lastTouched, c), EDGE_ACTIVE);
            }
        }
        // これから割る節点。次の手で何が起きるかが分かる
        if (st.splitDepth >= 0 && st.splitDepth < (int)st.route.size()) {
            graph->setNodeColor(st.route[st.splitDepth], NODE_FRONTIER);
        }
        if (st.cursor >= 0) graph->setNodeColor(st.cursor, NODE_VISITING);
    }

    void clearTree() {
        graph = std::make_unique<GraphData>(MAX_NODES, MAX_NODES * MAX_ORDER);
        graph->startNodeIndex = -1;
        keys.clear();
        children.clear();
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

    // 一番左の子を辿った深さ。どの葉も同じ深さなので、これが木の高さになる。
    int depthOf(int n) const {
        if (n < 0) return 0;
        int d = 1;
        while (!children[n].empty()) { n = children[n][0]; d++; }
        return d;
    }

protected:
    const char* labelMode() const override { return "text"; }

    bool handleCommand(const std::string& source, const std::string& input) override {
        if (source == "setValues") { setValuesFrom(input); return true; }
        if (source == "resetRun")  { resetRun(); return true; }
        if (source == "setOrder") {
            int m = DEFAULT_ORDER;
            std::istringstream iss(input);
            iss >> m;
            order = std::clamp(m, MIN_ORDER, MAX_ORDER);
            resetRun(); // 次数が変われば出来上がる木も変わる
            return true;
        }
        if (source == "genRandom") {
            int count = 10;
            std::istringstream iss(input);
            iss >> count;
            generateValues(count);
            return true;
        }
        return false;
    }

public:
    BTreeVisualizer() {
        // 基底のコンストラクタが一般グラフを作っているので、木用に置き換える
        layout = std::make_unique<TreeLayout>();
        weighted = false;
        hasNodeWeights = false;
        generatedDirected = false; // 木に矢印は要らない
        skipExtension = false;     // 割れる様子を見せたいので収束を飛ばさない
        setValuesFrom("10 20 30 40 50 60 70");
    }

    bool step() override {
        if (st.finished) return false;
        st.duplicate = false;
        st.splitting = false;

        // --- あふれた節点を割る ---
        if (st.splitDepth >= 0) {
            stepCount++;
            splitAt(st.splitDepth);
            st.splitting = true;
            if (st.splitDepth < 0) finishValue(); // これ以上あふれていない
            rebuildEdges();
            relayout();
            syncVisuals();
            return true;
        }

        // --- 次の値の挿入を始める ---
        if (st.cursor < 0) {
            if (st.pending >= (int)values.size()) {
                st.finished = true;
                st.lastTouched = -1;
                syncVisuals();
                return false;
            }
            stepCount++;
            st.lastTouched = -1;

            if (root < 0) {
                root = addNode(-1);
                keys[root].push_back(values[st.pending]);
                st.lastTouched = root;
                finishValue();
                rebuildEdges();
                relayout();
                syncVisuals();
                return true;
            }

            st.cursor = root;
            st.route.assign(1, root);
            syncVisuals();
            return true;
        }

        stepCount++;
        int v = values[st.pending];
        const std::vector<int>& ks = keys[st.cursor];

        // 同じ値は入れない
        if (std::find(ks.begin(), ks.end(), v) != ks.end()) {
            st.duplicate = true;
            st.lastTouched = st.cursor;
            finishValue();
            syncVisuals();
            return true;
        }

        // 値の並びの中で v が入る位置。降りる子も同じ位置で決まる
        std::size_t slot = 0;
        while (slot < ks.size() && ks[slot] < v) slot++;

        // --- 葉に入れる ---
        if (isLeaf(st.cursor)) {
            int leaf = st.cursor;
            keys[leaf].insert(keys[leaf].begin() + slot, v);
            st.lastTouched = leaf;
            st.cursor = -1;
            if (overflows(leaf)) st.splitDepth = (int)st.route.size() - 1;
            else finishValue();
            rebuildEdges();
            relayout();
            syncVisuals();
            return true;
        }

        // --- どの子へ降りるか決める ---
        st.cursor = children[st.cursor][slot];
        st.route.push_back(st.cursor);
        st.lastTouched = -1;
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

        // 丸の中に出す、値を並べた文字列
        emscripten::val ls = emscripten::val::array();
        for (int i = 0; i < (int)keys.size(); i++) ls.call<void>("push", labelOf(i));
        state.set("nodeLabels", ls);

        state.set("pending", st.pending);
        state.set("cursor", st.cursor);
        state.set("order", order);
        state.set("splitting", st.splitting);
        state.set("duplicate", st.duplicate);
        state.set("finished", st.finished);
        state.set("canStepBack", stepCount > 0);
        state.set("insertedCount", graph ? graph->nodeCount() : 0);
        state.set("maxValues", MAX_VALUES);
        state.set("treeHeight", depthOf(root));
        return state;
    }
};
