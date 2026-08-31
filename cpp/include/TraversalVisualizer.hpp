#pragma once
#include "GraphVisualizer.hpp"
#include "GraphColors.hpp"
#include <emscripten/val.h>
#include <string>
#include <sstream>
#include <vector>
#include <limits>
#include <algorithm>

// 幅優先探索 / 深さ優先探索 / ダイクストラ法のビジュアライザ。
//
// 3つを1つのクラスにしている。違いは
// 「まだ隣接辺を見終わっていない頂点の列 (frames) からどれを取り出すか」
// だけだからである。
//
//   frames は「まだ隣接辺を見終わっていない頂点」の列。
//   BFS      … 先頭を取る。新しく見つけた頂点は末尾に積むので後回し（キュー）
//   DFS      … 末尾を取る。新しく見つけた頂点は末尾に積むのですぐ潜る（スタック）
//   Dijkstra … 暫定距離が最小のものを取る（優先度付きキュー）
//
// この形にすると DFS は再帰と同じ前順走査になり、隣接を見終わった頂点が
// frames から外れる様子がそのままバックトラックの可視化になる。
//
// ダイクストラだけは「取り出した瞬間に最短距離が確定する」という性質が本質なので、
// 確定を独立した1ステップとして見せている。
class TraversalVisualizer : public GraphVisualizer {
public:
    enum Mode { BFS = 0, DFS = 1, DIJKSTRA = 2 };

private:
    // 保持するスナップショットの数。1つあたり O(V + E)。
    // 頂点100の完全グラフのような極端な入力でメモリが破裂しないよう蓋をする。
    static constexpr std::size_t HISTORY_LIMIT = 3000;

    static constexpr float INF = std::numeric_limits<float>::infinity();

    Mode mode = BFS;
    int startNode = 0;
    int goalNode = -1;

    // (行き先, 辺のインデックス)。無向グラフとして生成された場合は両向きに入れる。
    std::vector<std::vector<std::pair<int, int>>> adjTraversal;

    // ダイクストラは頂点の重み欄を暫定距離の表示に使うので、元の値を退避しておく
    std::vector<float> nodeWeightBackup;

    struct Frame {
        int vertex;
        int cursor; // adjTraversal[vertex] を何本目まで見たか
    };

    struct State {
        std::vector<Frame> frames;
        std::vector<char> inQueue;    // frames に入っているか
        std::vector<char> discovered;
        std::vector<int>  parentNode;
        std::vector<int>  parentEdge;
        std::vector<char> examined;   // 見に行った辺
        std::vector<int>  visitOrder;
        std::vector<int>  path;       // 見つかった経路の頂点列 (start -> goal)

        // ダイクストラ用
        std::vector<float> dist;
        std::vector<char>  settled;   // 最短距離が確定した

        bool finished = false;
        bool found = false;
    };

    State st;
    std::vector<State> history;

    int nodeCount() const { return graph ? graph->nodeCount() : 0; }
    int edgeCount() const { return graph ? graph->edgeCount() : 0; }

    float edgeWeight(int i) const {
        return graph->edgeData[i * GraphData::EDGE_STRIDE + 2];
    }

    // 取り出す要素の選び方。3つのアルゴリズムの違いはここに閉じている。
    int activeIndex() const {
        if (st.frames.empty()) return -1;
        if (mode == BFS) return 0;
        if (mode == DFS) return (int)st.frames.size() - 1;

        // ダイクストラ: 暫定距離が最小のもの。
        // 頂点数の上限が 100 なので、線形探索で十分速いうえに
        // 「未確定の中から最小を選ぶ」という説明そのままの形になる。
        int best = 0;
        for (int i = 1; i < (int)st.frames.size(); i++) {
            if (st.dist[st.frames[i].vertex] < st.dist[st.frames[best].vertex]) best = i;
        }
        return best;
    }

    void dropActive() {
        int i = activeIndex();
        if (i < 0) return;
        st.inQueue[st.frames[i].vertex] = 0;
        st.frames.erase(st.frames.begin() + i);
    }

    int activeVertex() const {
        int i = activeIndex();
        return i < 0 ? -1 : st.frames[i].vertex;
    }

    void pushFrame(int v) {
        st.frames.push_back({v, 0});
        st.inQueue[v] = 1;
    }

    void buildTraversalAdjacency() {
        int n = nodeCount();
        adjTraversal.assign(n, {});
        for (int i = 0; i < edgeCount(); i++) {
            int from = graph->edgeFrom(i);
            int to   = graph->edgeTo(i);
            if (from < 0 || from >= n || to < 0 || to >= n) continue;
            adjTraversal[from].push_back({to, i});
            // 無向グラフなら逆向きにも辿れる
            if (!generatedDirected && from != to) adjTraversal[to].push_back({from, i});
        }
    }

    void onGraphChanged() override {
        buildTraversalAdjacency();

        // 頂点の重み欄はダイクストラが暫定距離の表示に使うので、元の値を覚えておく
        nodeWeightBackup.assign(nodeCount(), 0.0f);
        for (int i = 0; i < nodeCount(); i++) {
            nodeWeightBackup[i] = graph->nodeData[i * GraphData::NODE_STRIDE + 2];
        }

        int n = nodeCount();
        if (startNode < 0 || startNode >= n) startNode = (n > 0 ? 0 : -1);
        if (goalNode >= n) goalNode = -1;
        resetTraversal();
    }

    void resetTraversal() {
        int n = nodeCount(), e = edgeCount();
        history.clear();

        st = State{};
        st.inQueue.assign(n, 0);
        st.discovered.assign(n, 0);
        st.parentNode.assign(n, -1);
        st.parentEdge.assign(n, -1);
        st.examined.assign(e, 0);
        st.dist.assign(n, INF);
        st.settled.assign(n, 0);

        if (startNode >= 0 && startNode < n) {
            st.discovered[startNode] = 1;
            st.dist[startNode] = 0.0f;
            pushFrame(startNode);
            // ダイクストラの訪問順は「確定した順」なので、取り出したときに積む。
            // BFS / DFS は発見順なのでここで積む。
            if (mode != DIJKSTRA) st.visitOrder.push_back(startNode);
            if (startNode == goalNode) {
                buildPath(startNode);
                st.finished = true;
                st.found = true;
            }
        } else {
            st.finished = true;
        }
        syncVisuals();
    }

    void buildPath(int goal) {
        st.path.clear();
        for (int v = goal; v != -1; v = st.parentNode[v]) {
            st.path.push_back(v);
            if (v == startNode) break;
        }
        std::reverse(st.path.begin(), st.path.end());
    }

    // 表示は状態から毎回導出する。別に持って更新し忘れる事故を構造的に防ぐ。
    void syncVisuals() {
        if (!graph) return;
        int n = nodeCount();

        // --- ノードの色 ---
        int active = activeVertex();
        for (int i = 0; i < n; i++) {
            int c = NODE_DEFAULT;
            if (st.discovered[i]) c = st.inQueue[i] ? NODE_FRONTIER : NODE_VISITED;
            graph->setNodeColor(i, c);
        }
        if (active >= 0 && active < n) graph->setNodeColor(active, NODE_VISITING);

        // --- 辺の色 ---
        // 探索木の辺は parentEdge から導出する。ダイクストラは緩和のたびに
        // 親が張り替わるので、色を持たずに毎回引き直すのが唯一の正解になる。
        for (int i = 0; i < edgeCount(); i++) {
            graph->setEdgeColor(i, st.examined[i] ? EDGE_VISITED : EDGE_DEFAULT);
        }
        for (int v = 0; v < n; v++) {
            if (!st.discovered[v]) continue;
            int e = st.parentEdge[v];
            if (e >= 0 && e < edgeCount()) graph->setEdgeColor(e, EDGE_TREE);
        }

        // --- 経路は最優先で上書き ---
        for (int v : st.path) {
            if (v >= 0 && v < n) graph->setNodeColor(v, NODE_PATH);
            int e = st.parentEdge[v];
            if (e >= 0 && e < edgeCount()) graph->setEdgeColor(e, EDGE_PATH);
        }

        // --- 頂点の脇に出す数値 ---
        // ダイクストラのときは暫定距離、それ以外は入力された頂点の重み。
        for (int i = 0; i < n && i < (int)nodeWeightBackup.size(); i++) {
            graph->nodeData[i * GraphData::NODE_STRIDE + 2] =
                (mode == DIJKSTRA) ? st.dist[i] : nodeWeightBackup[i];
        }
    }

    void pushHistory() {
        history.push_back(st);
        if (history.size() > HISTORY_LIMIT) history.erase(history.begin());
    }

    void finishFound(int v) {
        buildPath(v);
        st.finished = true;
        st.found = true;
    }

protected:
    // 探索中は頂点の重み欄に暫定距離を出しているので、
    // テキスト化には退避しておいた元の値を使ってもらう。
    const std::vector<float>* originalNodeWeights() const override { return &nodeWeightBackup; }

    bool handleCommand(const std::string& source, const std::string& input) override {
        if (source == "setTraversal") {
            // "bfs 0 5" / "dfs 3 -1" / "dijkstra 0 7"
            std::istringstream iss(input);
            std::string m;
            int s = 0, g = -1;
            iss >> m >> s;
            if (!(iss >> g)) g = -1;

            if (m == "dfs" || m == "DFS")                     mode = DFS;
            else if (m == "dijkstra" || m == "Dijkstra")      mode = DIJKSTRA;
            else                                              mode = BFS;

            int n = nodeCount();
            startNode = (s >= 0 && s < n) ? s : (n > 0 ? 0 : -1);
            goalNode  = (g >= 0 && g < n) ? g : -1;
            resetTraversal();
            return true;
        }
        if (source == "resetTraversal") {
            resetTraversal();
            return true;
        }
        return false;
    }

public:
    bool step() override {
        if (st.finished) return false;
        pushHistory();

        int ai = activeIndex();
        if (ai < 0) {
            // 見るべき頂点が尽きた = 到達できる範囲を調べ終えた
            st.finished = true;
            syncVisuals();
            return false;
        }

        const int u = st.frames[ai].vertex;

        // ダイクストラは「取り出した瞬間に最短距離が確定する」のが本質なので、
        // 確定を独立した1ステップとして見せる。
        if (mode == DIJKSTRA && !st.settled[u]) {
            st.settled[u] = 1;
            st.visitOrder.push_back(u);
            if (u == goalNode) {
                finishFound(u);
                syncVisuals();
                return false;
            }
            syncVisuals();
            return true;
        }

        // frames は push_back で伸びるので参照は持たず、値を先に取り出す。
        const int cursor = st.frames[ai].cursor;
        const auto& neighbors = adjTraversal[u];

        if (cursor < (int)neighbors.size()) {
            // 隣接辺を1本だけ調べる
            const int to = neighbors[cursor].first;
            const int ei = neighbors[cursor].second;
            st.frames[ai].cursor++;
            st.examined[ei] = 1;

            if (mode == DIJKSTRA) {
                // 緩和: この辺を通った方が近ければ距離と親を張り替える
                float nd = st.dist[u] + edgeWeight(ei);
                if (!st.settled[to] && nd < st.dist[to]) {
                    st.dist[to] = nd;
                    st.parentNode[to] = u;
                    st.parentEdge[to] = ei;
                    if (!st.discovered[to]) st.discovered[to] = 1;
                    if (!st.inQueue[to]) pushFrame(to);
                }
            } else if (!st.discovered[to]) {
                st.discovered[to] = 1;
                st.parentNode[to] = u;
                st.parentEdge[to] = ei;
                pushFrame(to);
                st.visitOrder.push_back(to);

                if (to == goalNode) finishFound(to);
            }

            syncVisuals();
            return !st.finished;
        }

        // この頂点の隣接をすべて調べ終えた。
        // DFS ではこれがバックトラックにあたる。
        dropActive();
        syncVisuals();
        if (st.frames.empty()) st.finished = true;
        return !st.finished;
    }

    void stepBack() override {
        if (history.empty()) return;
        st = history.back();
        history.pop_back();
        syncVisuals();
    }

    emscripten::val getState(emscripten::val params) override {
        emscripten::val state = GraphVisualizer::getState(params);

        const char* names[] = {"bfs", "dfs", "dijkstra"};
        state.set("algorithm", std::string(names[mode]));
        state.set("startNode", startNode);
        state.set("goalNode", goalNode);
        state.set("current", activeVertex());
        state.set("finished", st.finished);
        state.set("found", st.found);
        state.set("canStepBack", !history.empty());
        // 頂点の脇の数字が何を表しているか
        state.set("nodeValueMode", std::string(mode == DIJKSTRA ? "distance" : "weight"));

        // ダイクストラは負の重みを前提にしていない。混ざっていたら UI から知らせる。
        bool negative = false;
        for (int i = 0; i < edgeCount(); i++) if (edgeWeight(i) < 0) { negative = true; break; }
        state.set("hasNegativeEdge", negative);

        // 進行状況の配列は要求されたときだけ組み立てる。
        // 描画ループは毎フレーム getState を呼ぶので、常に作ると無駄が大きい。
        if (!(params.hasOwnProperty("withProgress") && params["withProgress"].as<bool>())) {
            return state;
        }

        // フロンティア（キュー / スタック / 優先度付きキュー）の中身を、
        // 取り出される順に並べる。処理中の頂点は取り出し済みなので含めない。
        const int ai = activeIndex();
        std::vector<int> order;
        for (int i = 0; i < (int)st.frames.size(); i++) {
            if (i != ai) order.push_back(i);
        }
        if (mode == DFS) {
            std::reverse(order.begin(), order.end());
        } else if (mode == DIJKSTRA) {
            std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
                return st.dist[st.frames[a].vertex] < st.dist[st.frames[b].vertex];
            });
        }

        emscripten::val frontier = emscripten::val::array();
        for (int i : order) frontier.call<void>("push", st.frames[i].vertex);
        state.set("frontier", frontier);

        emscripten::val visits = emscripten::val::array();
        for (int v : st.visitOrder) visits.call<void>("push", v);
        state.set("visitOrder", visits);

        emscripten::val path = emscripten::val::array();
        for (int v : st.path) path.call<void>("push", v);
        state.set("path", path);

        if (mode == DIJKSTRA) {
            emscripten::val dists = emscripten::val::array();
            for (float d : st.dist) dists.call<void>("push", d);
            state.set("distances", dists);
            // 経路長 (終点まで届いていれば その距離)
            if (goalNode >= 0 && goalNode < nodeCount()) {
                state.set("goalDistance", st.dist[goalNode]);
            }
        }

        return state;
    }
};
