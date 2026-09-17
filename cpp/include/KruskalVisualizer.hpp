#pragma once
#include "GraphVisualizer.hpp"
#include "GraphColors.hpp"
#include <emscripten/val.h>
#include <algorithm>
#include <numeric>
#include <string>
#include <vector>

// クラスカル法 (最小全域木) のビジュアライザ。
//
// 辺を軽い順に1本ずつ見て、両端が別の木なら採用、同じ木なら捨てる。
// 1本の辺は2手: 「見る」(橙) → 「決める」(採用は緑、却下は灰)。
// 間に一拍置くのは、採用されるかを見る側が予想できるようにするため。
//
// V-1 本そろっても打ち切らず、辺は最後まで全部見る。残りが全部却下される
// 様子も動きを見ないと分からないことなので、素朴な形のままにしている。
//
// Union-Find の中身 (上がる / 圧縮) はここでは見せない。それは Union-Find の
// ページが担う。ここで見えるべきなのは「軽い順」と「閉路になる辺が捨てられる」。
class KruskalVisualizer : public GraphVisualizer {
    static constexpr std::size_t HISTORY_LIMIT = 3000;

    enum Status : char { PENDING = 0, ACCEPTED = 1, REJECTED = 2 };

    struct State {
        std::vector<int>  order;        // 辺の番号を (重み, 番号) の昇順に
        int cursor = 0;                 // order の何番目か
        bool looking = false;           // 「見る」の手を終えて「決める」を待つ
        std::vector<int>  parent, size; // Union-Find
        std::vector<char> status;       // 辺ごと
        int   treeEdges = 0;
        float treeWeight = 0.0f;
        int   lastEdge = -1;            // 直前に決めた辺
        bool  finished = false;
    };

    State st;
    std::vector<State> history;

    int nodeCount() const { return graph ? graph->nodeCount() : 0; }
    int edgeCount() const { return graph ? graph->edgeCount() : 0; }
    float edgeWeight(int i) const { return graph->edgeData[i * GraphData::EDGE_STRIDE + 2]; }

    int find(int x) {
        while (st.parent[x] != x) {
            st.parent[x] = st.parent[st.parent[x]];
            x = st.parent[x];
        }
        return x;
    }

    void unite(int a, int b) {
        if (st.size[a] < st.size[b]) std::swap(a, b);
        st.parent[b] = a;
        st.size[a] += st.size[b];
    }

    void onGraphChanged() override { resetRun(); }

    void resetRun() {
        history.clear();
        st = State{};
        int n = nodeCount(), e = edgeCount();
        st.order.resize(e);
        std::iota(st.order.begin(), st.order.end(), 0);
        std::stable_sort(st.order.begin(), st.order.end(), [&](int a, int b) {
            return edgeWeight(a) < edgeWeight(b);
        });
        st.parent.resize(n);
        std::iota(st.parent.begin(), st.parent.end(), 0);
        st.size.assign(n, 1);
        st.status.assign(e, PENDING);
        st.finished = (e == 0);
        syncVisuals();
    }

    // 今見ている辺 (見る → 決める の間) か、直前に決めた辺
    int currentEdge() const {
        if (st.looking) return st.order[st.cursor];
        return st.lastEdge;
    }

    // 色は状態から毎回作り直す
    void syncVisuals() {
        if (!graph) return;
        graph->resetColors();
        for (int i = 0; i < edgeCount(); i++) {
            graph->setEdgeColor(i, st.status[i] == ACCEPTED ? EDGE_TREE
                                 : st.status[i] == REJECTED ? EDGE_VISITED : EDGE_DEFAULT);
        }
        int cur = currentEdge();
        if (cur < 0) return;
        int u = graph->edgeFrom(cur), v = graph->edgeTo(cur);
        if (st.looking) {
            graph->setEdgeColor(cur, EDGE_ACTIVE);
            graph->setNodeColor(u, NODE_VISITING);
            graph->setNodeColor(v, NODE_VISITING);
        } else {
            int c = st.status[cur] == ACCEPTED ? NODE_PATH : NODE_VISITED;
            graph->setNodeColor(u, c);
            graph->setNodeColor(v, c);
        }
    }

    void pushHistory() {
        history.push_back(st);
        if (history.size() > HISTORY_LIMIT) history.erase(history.begin());
    }

    int setCount() {
        int c = 0;
        for (int i = 0; i < nodeCount(); i++) if (st.parent[i] == i) c++;
        return c;
    }

protected:
    bool handleCommand(const std::string& source, const std::string& input) override {
        (void)input;
        if (source == "resetRun") { resetRun(); return true; }
        return false;
    }

public:
    KruskalVisualizer() {
        weighted = true;
        generatedDirected = false;
        resetRun();
    }

    bool step() override {
        if (st.finished) return false;
        pushHistory();

        int e = st.order[st.cursor];
        if (!st.looking) {
            // 見る
            st.looking = true;
            st.lastEdge = -1;
            syncVisuals();
            return true;
        }

        // 決める
        int a = find(graph->edgeFrom(e)), b = find(graph->edgeTo(e));
        if (a != b) {
            unite(a, b);
            st.status[e] = ACCEPTED;
            st.treeEdges++;
            st.treeWeight += edgeWeight(e);
        } else {
            st.status[e] = REJECTED;
        }
        st.lastEdge = e;
        st.looking = false;
        st.cursor++;
        if (st.cursor >= (int)st.order.size()) st.finished = true;
        syncVisuals();
        return true;
    }

    void stepBack() override {
        if (history.empty()) return;
        st = history.back();
        history.pop_back();
        syncVisuals();
    }

    emscripten::val getState(emscripten::val params) override {
        emscripten::val state = GraphVisualizer::getState(params);
        state.set("finished", st.finished);
        state.set("canStepBack", !history.empty());
        state.set("current", currentEdge());
        state.set("looking", st.looking);
        state.set("decision", std::string(
            st.looking || st.lastEdge < 0 ? ""
            : st.status[st.lastEdge] == ACCEPTED ? "accept" : "reject"));
        state.set("treeEdges", st.treeEdges);
        state.set("treeWeight", st.treeWeight);
        state.set("setCount", setCount());

        if (!(params.hasOwnProperty("withProgress") && params["withProgress"].as<bool>())) {
            return state;
        }
        emscripten::val order = emscripten::val::array();
        for (int i : st.order) order.call<void>("push", i);
        state.set("edgeOrder", order);
        emscripten::val status = emscripten::val::array();
        for (char s : st.status) status.call<void>("push", (int)s);
        state.set("edgeStatus", status);
        return state;
    }
};
