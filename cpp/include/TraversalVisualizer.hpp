#pragma once
#include "GraphVisualizer.hpp"
#include "GraphColors.hpp"
#include <emscripten/val.h>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>

// 幅優先探索 / 深さ優先探索のビジュアライザ。
//
// BFS と DFS を1つのクラスにしている。両者の違いは
// 「処理中の頂点の並び (frames) のどちら端を見るか」だけだからである。
//
//   frames は「まだ隣接辺を見終わっていない頂点」の列。
//   BFS … 先頭を見る。新しく見つけた頂点は末尾に積むので、後回しになる（キュー）
//   DFS … 末尾を見る。新しく見つけた頂点は末尾に積むので、すぐ潜る（スタック）
//
// この形にすると DFS は再帰と同じ前順走査になり、隣接を見終わった頂点が
// frames から外れる様子がそのままバックトラックの可視化になる。
class TraversalVisualizer : public GraphVisualizer {
public:
    enum Mode { BFS = 0, DFS = 1 };

private:
    // 保持するスナップショットの数。1つあたり O(V + E)。
    // 頂点100の完全グラフのような極端な入力でメモリが破裂しないよう蓋をする。
    static constexpr std::size_t HISTORY_LIMIT = 3000;

    Mode mode = BFS;
    int startNode = 0;
    int goalNode = -1;

    // (行き先, 辺のインデックス)。無向グラフとして生成された場合は両向きに入れる。
    std::vector<std::vector<std::pair<int, int>>> adjTraversal;

    struct Frame {
        int vertex;
        int cursor; // adjTraversal[vertex] を何本目まで見たか
    };

    struct State {
        std::vector<Frame> frames;
        std::vector<char> discovered;
        std::vector<int>  parentNode;
        std::vector<int>  parentEdge;
        std::vector<char> edgeMark;   // EdgeColor の値
        std::vector<int>  visitOrder;
        std::vector<int>  path;       // 見つかった経路の頂点列 (start -> goal)
        bool finished = false;
        bool found = false;
    };

    State st;
    std::vector<State> history;

    int nodeCount() const { return graph ? graph->nodeCount() : 0; }
    int edgeCount() const { return graph ? graph->edgeCount() : 0; }

    // BFS は先頭、DFS は末尾を見る。両者の差はここと dropActive だけ。
    int activeIndex() const {
        if (st.frames.empty()) return -1;
        return mode == BFS ? 0 : (int)st.frames.size() - 1;
    }

    void dropActive() {
        if (st.frames.empty()) return;
        if (mode == BFS) st.frames.erase(st.frames.begin());
        else             st.frames.pop_back();
    }

    int activeVertex() const {
        int i = activeIndex();
        return i < 0 ? -1 : st.frames[i].vertex;
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
        int n = nodeCount();
        if (startNode < 0 || startNode >= n) startNode = (n > 0 ? 0 : -1);
        if (goalNode >= n) goalNode = -1;
        resetTraversal();
    }

    void resetTraversal() {
        int n = nodeCount(), e = edgeCount();
        history.clear();

        st = State{};
        st.discovered.assign(n, 0);
        st.parentNode.assign(n, -1);
        st.parentEdge.assign(n, -1);
        st.edgeMark.assign(e, (char)EDGE_DEFAULT);

        if (startNode >= 0 && startNode < n) {
            st.discovered[startNode] = 1;
            st.frames.push_back({startNode, 0});
            st.visitOrder.push_back(startNode);
            if (startNode == goalNode) {
                buildPath(startNode);
                st.finished = true;
                st.found = true;
            }
        } else {
            st.finished = true;
        }
        syncColors();
    }

    void buildPath(int goal) {
        st.path.clear();
        for (int v = goal; v != -1; v = st.parentNode[v]) {
            st.path.push_back(v);
            if (v == startNode) break;
        }
        std::reverse(st.path.begin(), st.path.end());

        for (int v : st.path) {
            int e = st.parentEdge[v];
            if (e >= 0 && e < (int)st.edgeMark.size()) st.edgeMark[e] = (char)EDGE_PATH;
        }
    }

    // 色は状態から毎回導出する。色を別に持って更新し忘れる事故を防ぐ。
    void syncColors() {
        if (!graph) return;
        int n = nodeCount();

        std::vector<char> inFrames(n, 0);
        for (const Frame& f : st.frames) {
            if (f.vertex >= 0 && f.vertex < n) inFrames[f.vertex] = 1;
        }
        int active = activeVertex();

        for (int i = 0; i < n; i++) {
            int c = NODE_DEFAULT;
            if (st.discovered[i]) c = inFrames[i] ? NODE_FRONTIER : NODE_VISITED;
            graph->setNodeColor(i, c);
        }
        if (active >= 0 && active < n) graph->setNodeColor(active, NODE_VISITING);

        for (int i = 0; i < edgeCount() && i < (int)st.edgeMark.size(); i++) {
            graph->setEdgeColor(i, st.edgeMark[i]);
        }

        // 経路が確定していれば最優先で上書きする
        for (int v : st.path) {
            if (v >= 0 && v < n) graph->setNodeColor(v, NODE_PATH);
        }
    }

    void pushHistory() {
        history.push_back(st);
        if (history.size() > HISTORY_LIMIT) history.erase(history.begin());
    }

protected:
    bool handleCommand(const std::string& source, const std::string& input) override {
        if (source == "setTraversal") {
            // "bfs 0 5" / "dfs 3 -1"
            std::istringstream iss(input);
            std::string m;
            int s = 0, g = -1;
            iss >> m >> s;
            if (!(iss >> g)) g = -1;

            mode = (m == "dfs" || m == "DFS") ? DFS : BFS;
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
            syncColors();
            return false;
        }

        // frames は push_back で伸びるので参照は持たず、値を先に取り出す。
        const int u = st.frames[ai].vertex;
        const int cursor = st.frames[ai].cursor;
        const auto& neighbors = adjTraversal[u];

        if (cursor < (int)neighbors.size()) {
            // 隣接辺を1本だけ調べる
            const int to = neighbors[cursor].first;
            const int ei = neighbors[cursor].second;
            st.frames[ai].cursor++;

            if (!st.discovered[to]) {
                st.discovered[to] = 1;
                st.parentNode[to] = u;
                st.parentEdge[to] = ei;
                st.edgeMark[ei] = (char)EDGE_TREE;
                st.frames.push_back({to, 0});
                st.visitOrder.push_back(to);

                if (to == goalNode) {
                    buildPath(to);
                    st.finished = true;
                    st.found = true;
                }
            } else if (st.edgeMark[ei] == (char)EDGE_DEFAULT) {
                // 既に見つけている頂点へ向かう辺。木には入らない
                st.edgeMark[ei] = (char)EDGE_VISITED;
            }

            syncColors();
            return !st.finished;
        }

        // この頂点の隣接をすべて調べ終えた。
        // DFS ではこれがバックトラックにあたる。
        dropActive();
        syncColors();
        if (st.frames.empty()) st.finished = true;
        return !st.finished;
    }

    void stepBack() override {
        if (history.empty()) return;
        st = history.back();
        history.pop_back();
        syncColors();
    }

    emscripten::val getState(emscripten::val params) override {
        emscripten::val state = GraphVisualizer::getState(params);

        state.set("algorithm", std::string(mode == BFS ? "bfs" : "dfs"));
        state.set("startNode", startNode);
        state.set("goalNode", goalNode);
        state.set("current", activeVertex());
        state.set("finished", st.finished);
        state.set("found", st.found);
        state.set("canStepBack", !history.empty());

        // フロンティア（キュー / スタック）の中身。
        // 取り出される順に並べて渡す。
        emscripten::val frontier = emscripten::val::array();
        if (mode == BFS) {
            for (const Frame& f : st.frames) frontier.call<void>("push", f.vertex);
        } else {
            for (auto it = st.frames.rbegin(); it != st.frames.rend(); ++it) {
                frontier.call<void>("push", it->vertex);
            }
        }
        state.set("frontier", frontier);

        emscripten::val order = emscripten::val::array();
        for (int v : st.visitOrder) order.call<void>("push", v);
        state.set("visitOrder", order);

        emscripten::val path = emscripten::val::array();
        for (int v : st.path) path.call<void>("push", v);
        state.set("path", path);

        return state;
    }
};
