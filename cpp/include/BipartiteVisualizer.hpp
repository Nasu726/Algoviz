#pragma once
#include "GraphVisualizer.hpp"
#include "GraphColors.hpp"
#include <emscripten/val.h>
#include <string>
#include <utility>
#include <vector>

// 二部グラフ判定のビジュアライザ。
//
// BFS で2色に塗っていく。1手 = 辺を1本見る (BFS と同じ刻み)。
//   成分を始める … 塗られていない頂点のうち番号が最小のものを青で塗ってキューへ
//   辺を1本見る   … 隣が未着色なら反対の色で塗ってキューへ。同じ色なら二部グラフではない
//   見終える      … 隣を全部見た頂点をキューから外す
// キューが空になったら次の成分を始めるので、非連結でも全頂点を見る。
//
// 向きと重みはこの判定に関係ないので、無向として扱う。
// 処理中の頂点は色を変えず、この手で見た辺で示す。頂点を赤で塗ると、
// 赤緑の色覚では側の色の橙と紛れるため。
class BipartiteVisualizer : public GraphVisualizer {
    static constexpr std::size_t HISTORY_LIMIT = 3000;

    enum Status : char { PENDING = 0, TREE = 1, OK = 2, CONFLICT = 3 };

    struct Frame {
        int vertex;
        int cursor; // adj[vertex] を何本目まで見たか
    };

    struct State {
        std::vector<int>   side;        // -1: 未着色 / 0: 青 / 1: 橙
        std::vector<Frame> frames;      // キュー。先頭が処理中
        std::vector<char>  status;      // 辺ごと
        int  nextStart = 0;             // 次の成分を始める候補
        int  lookingEdge = -1;          // この手で見た辺
        int  conflictEdge = -1;
        int  componentCount = 0;
        bool finished = false;
        bool bipartite = false;
    };

    State st;
    std::vector<State> history;

    // (行き先, 辺の番号)。向きは見ないので両向きに入れる
    std::vector<std::vector<std::pair<int, int>>> adj;

    int nodeCount() const { return graph ? graph->nodeCount() : 0; }
    int edgeCount() const { return graph ? graph->edgeCount() : 0; }

    void buildAdjacency() {
        int n = nodeCount();
        adj.assign(n, {});
        for (int i = 0; i < edgeCount(); i++) {
            int from = graph->edgeFrom(i), to = graph->edgeTo(i);
            if (from < 0 || from >= n || to < 0 || to >= n) continue;
            adj[from].push_back({to, i});
            if (from != to) adj[to].push_back({from, i});
        }
    }

    // 塗られていない頂点まで nextStart を進める。残っていれば true
    bool advanceNextStart() {
        while (st.nextStart < nodeCount() && st.side[st.nextStart] != -1) st.nextStart++;
        return st.nextStart < nodeCount();
    }

    void onGraphChanged() override {
        buildAdjacency();
        resetRun();
    }

    void resetRun() {
        history.clear();
        st = State{};
        st.side.assign(nodeCount(), -1);
        st.status.assign(edgeCount(), PENDING);
        // 頂点が無ければ、塗る前から二部グラフ
        if (!advanceNextStart()) {
            st.finished = true;
            st.bipartite = true;
        }
        syncVisuals();
    }

    // 色は状態から毎回作り直す
    void syncVisuals() {
        if (!graph) return;
        graph->resetColors();
        for (int v = 0; v < nodeCount(); v++) {
            if (st.side[v] >= 0) graph->setNodeColor(v, st.side[v] == 0 ? NODE_SIDE_A : NODE_SIDE_B);
        }
        for (int i = 0; i < edgeCount(); i++) {
            graph->setEdgeColor(i, st.status[i] == TREE ? EDGE_TREE
                                 : st.status[i] == OK ? EDGE_VISITED
                                 : st.status[i] == CONFLICT ? EDGE_ACTIVE : EDGE_DEFAULT);
        }
        if (st.lookingEdge >= 0) graph->setEdgeColor(st.lookingEdge, EDGE_ACTIVE);
    }

    void pushHistory() {
        history.push_back(st);
        if (history.size() > HISTORY_LIMIT) history.erase(history.begin());
    }

    int current() const {
        return st.finished || st.frames.empty() ? -1 : st.frames.front().vertex;
    }

protected:
    bool handleCommand(const std::string& source, const std::string& input) override {
        (void)input;
        if (source == "resetRun") { resetRun(); return true; }
        return false;
    }

public:
    BipartiteVisualizer() {
        weighted = false;
        generatedDirected = false;
        onGraphChanged();
    }

    bool step() override {
        if (st.finished) return false;
        pushHistory();
        st.lookingEdge = -1;

        // 成分を始める。nextStart は塗られていない頂点を指している
        if (st.frames.empty()) {
            int s = st.nextStart;
            st.side[s] = 0;
            st.componentCount++;
            st.frames.push_back({s, 0});
            syncVisuals();
            return true;
        }

        const int u = st.frames.front().vertex;
        const int cursor = st.frames.front().cursor;

        if (cursor < (int)adj[u].size()) {
            // 辺を1本見る
            const int to = adj[u][cursor].first;
            const int ei = adj[u][cursor].second;
            st.frames.front().cursor++;
            st.lookingEdge = ei;

            if (st.side[to] == -1) {
                st.side[to] = 1 - st.side[u];
                st.status[ei] = TREE;
                st.frames.push_back({to, 0});
            } else if (st.side[to] == st.side[u]) {
                st.status[ei] = CONFLICT;
                st.conflictEdge = ei;
                st.finished = true;
                st.bipartite = false;
            } else if (st.status[ei] == PENDING) {
                // 無向なので1本の辺は両端から2回見る。色を伝えた辺はそのまま残す
                st.status[ei] = OK;
            }
            syncVisuals();
            return true;
        }

        // 見終える
        st.frames.erase(st.frames.begin());
        if (st.frames.empty() && !advanceNextStart()) {
            st.finished = true;
            st.bipartite = true;
        }
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
        int colored = 0;
        for (int s : st.side) if (s >= 0) colored++;

        state.set("algorithm", std::string("bipartite"));
        state.set("current", current());
        state.set("finished", st.finished);
        state.set("bipartite", st.bipartite);
        state.set("lookingEdge", st.lookingEdge);
        state.set("conflictEdge", st.conflictEdge);
        state.set("componentCount", st.componentCount);
        state.set("coloredCount", colored);
        state.set("canStepBack", !history.empty());

        if (!(params.hasOwnProperty("withProgress") && params["withProgress"].as<bool>())) {
            return state;
        }
        // キューの中身。処理中 (先頭) は含めない。終わったら空
        emscripten::val frontier = emscripten::val::array();
        if (!st.finished) {
            for (std::size_t i = 1; i < st.frames.size(); i++) frontier.call<void>("push", st.frames[i].vertex);
        }
        state.set("frontier", frontier);

        emscripten::val sides = emscripten::val::array();
        for (int s : st.side) sides.call<void>("push", s);
        state.set("sides", sides);
        return state;
    }
};
