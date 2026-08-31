#pragma once
#include "IVisualizer.hpp"
#include "GraphData.hpp"
#include "GeneralGraphLayout.hpp"
#include <emscripten/val.h>
#include <memory>
#include <sstream>
#include <string>
#include <random>
#include <algorithm>
#include <vector>

// 一般グラフの「生成 / レイアウト / 描画データの供給」を担う基底クラス。
//
// このクラス自身はアルゴリズムを実行しない (step() は常に false を返す)。
// BFS・DFS などの探索や、オートマトン表示はこれを継承して差分だけを足す。
//
// step() と prepare() の役割分担:
//   prepare() … レイアウトの収束計算。描画ループが毎フレーム呼ぶ。
//   step()    … アルゴリズムの1手。再生コントロールが呼ぶ。
class GraphVisualizer : public IVisualizer {
public:
    // 頂点数・辺数の上限。C++ 側を唯一の情報源とし、JS へは getState で伝える。
    // 初期配置に使うヤコビ法が O(n^3) 級なので、これ以上は現実的な時間で収束しない。
    static constexpr int MAX_NODES = 100;
    static constexpr int MAX_EDGES = 1000;

    // レイアウト収束を諦めるまでのフレーム数。
    // WASM はメインスレッドで同期実行されるので、上限が無いとタブが固まる。
    static constexpr int LAYOUT_FRAME_LIMIT = 3000;

protected:
    std::unique_ptr<GraphData> graph;
    GeneralGraphLayout layout;
    bool skipExtension = true;
    bool generatedDirected = false;

    // 向きを無視し、自己ループを除いた隣接リスト。レイアウトが使う。
    // 以前は GeneralGraphLayout::init がローカルに作って捨てていたものを一元化した。
    std::vector<std::vector<int>> adjUndirected;

    std::mt19937 rng{std::random_device{}()};

    int randInt(int n) { return n <= 0 ? 0 : (int)(rng() % (unsigned)n); }

    void scatterNode(int i) {
        graph->setNode(i, (float)(randInt(600) + 100), (float)(randInt(400) + 100), 0, 0);
    }

    // オートマトンのように、常に有向として扱いたい派生クラスが true を返す
    virtual bool forceDirected() const { return false; }

    // グラフが差し替わったときに派生クラスが状態をリセットするためのフック
    virtual void onGraphChanged() {}

    void buildAdjacency() {
        int n = graph->nodeCount();
        adjUndirected.assign(n, {});
        for (int i = 0; i < graph->edgeCount(); i++) {
            int from = graph->edgeFrom(i);
            int to   = graph->edgeTo(i);
            if (from == to) continue;
            // 範囲外の頂点番号は無視する。テキスト入力から不正な辺が来ても
            // ここで弾かないと隣接リストの範囲外アクセスになる。
            if (from < 0 || from >= n || to < 0 || to >= n) continue;
            adjUndirected[from].push_back(to);
            adjUndirected[to].push_back(from);
        }
    }

    // 隣接リストとレイアウトを作り直す。コンストラクタからも呼べるよう
    // 仮想関数を含まない。(コンストラクタ中の仮想呼び出しは派生へ届かない)
    void rebuildLayout() {
        buildAdjacency();
        layout.init(graph.get(), adjUndirected);
        layout.is_stable = false;
    }

    // グラフを差し替えたあとに呼ぶ。派生クラスへの通知まで行う。
    void rebuild() {
        rebuildLayout();
        onGraphChanged();
    }

    // ==========================================
    // グラフ生成
    // ==========================================

    void generateRandom(int v, int e, bool allowSelfLoop, bool allowSameEdge, bool isDirected) {
        isDirected = isDirected || forceDirected();
        v = std::clamp(v, 0, MAX_NODES);
        e = std::clamp(e, 0, MAX_EDGES);
        generatedDirected = isDirected;
        graph = std::make_unique<GraphData>(v, e);
        for (int i = 0; i < v; i++) scatterNode(i);

        std::vector<std::pair<int, int>> possible;
        for (int i = 0; i < v; i++) {
            for (int j = (isDirected ? 0 : i); j < v; j++) {
                if (!allowSelfLoop && i == j) continue;
                possible.push_back({i, j});
            }
        }
        if (possible.empty()) return;

        if (allowSameEdge) {
            for (int i = 0; i < e; i++) {
                const auto& p = possible[randInt((int)possible.size())];
                graph->addEdge((float)p.first, (float)p.second, (float)randInt(100), 0);
            }
        } else {
            std::shuffle(possible.begin(), possible.end(), rng);
            int actual = std::min((int)possible.size(), e);
            for (int i = 0; i < actual; i++) {
                graph->addEdge((float)possible[i].first, (float)possible[i].second,
                               (float)randInt(100), 0);
            }
        }
    }

    void generateComplete(int v, bool isDirected) {
        isDirected = isDirected || forceDirected();
        v = std::clamp(v, 0, MAX_NODES);
        generatedDirected = isDirected;
        int e = isDirected ? v * (v - 1) : v * (v - 1) / 2;
        graph = std::make_unique<GraphData>(v, std::min(e, MAX_EDGES));
        for (int i = 0; i < v; i++) scatterNode(i);
        for (int i = 0; i < v; i++) {
            for (int j = (isDirected ? 0 : i + 1); j < v; j++) {
                if (i == j) continue;
                graph->addEdge((float)i, (float)j, (float)randInt(100), 0);
            }
        }
    }

    // "V E" の行に続けて "from to [重み]" が E 行。重みは省略できる。
    bool generateCustom(std::istringstream& iss) {
        int v = 0, e = 0;
        if (!(iss >> v >> e)) return false;
        v = std::clamp(v, 0, MAX_NODES);
        e = std::clamp(e, 0, MAX_EDGES);
        graph = std::make_unique<GraphData>(v, e);
        for (int i = 0; i < v; i++) graph->setNode(i, (float)i, (float)i, 0, 0);

        std::string line;
        std::getline(iss, line); // "V E" の行の残りを読み捨てる

        int added = 0;
        while (added < e && std::getline(iss, line)) {
            std::istringstream ls(line);
            int from, to;
            float weight = 0;
            if (!(ls >> from >> to)) continue; // 空行や不正な行は飛ばす
            ls >> weight;                      // 省略時は 0 のまま
            if (from < 0 || from >= v || to < 0 || to >= v) continue;
            graph->addEdge((float)from, (float)to, weight, 0);
            added++;
        }
        return true;
    }

    // 派生クラスが独自のコマンドを受け取るための拡張点。
    // 処理したら true を返す。
    virtual bool handleCommand(const std::string& source, const std::string& input) {
        (void)source; (void)input;
        return false;
    }

    std::string buildGraphText() const {
        std::ostringstream oss;
        int v = graph->nodeCount(), e = graph->edgeCount();
        oss << v << " " << e << "\n";
        for (int i = 0; i < e; i++) {
            oss << graph->edgeFrom(i) << " " << graph->edgeTo(i) << " "
                << graph->edgeData[i * GraphData::EDGE_STRIDE + 2] << "\n";
        }
        return oss.str();
    }

public:
    GraphVisualizer() {
        generateRandom(5, 7, false, false, false);
        rebuildLayout(); // コンストラクタなので仮想フックは呼ばない
    }

    // source: レイアウトの指向性、または派生クラス向けのコマンド名
    // input : グラフ生成コマンド ("random" / "complete" / "custom")
    void load(const std::string& source, const std::string& input) override {
        if (source == "horizontal")     layout.preferHorizontal = true;
        else if (source == "vertical")  layout.preferHorizontal = false;
        else if (handleCommand(source, input)) return;

        if (input.empty()) {
            layout.is_stable = false;
            return;
        }

        std::istringstream iss(input);
        std::string cmd;
        iss >> cmd;

        if (cmd == "random") {
            int v = 0, e = 0, skip = 1, selfLoop = 0, sameEdge = 0, dir = 0;
            if (!(iss >> v >> e)) return;
            iss >> skip >> selfLoop >> sameEdge >> dir;
            skipExtension = (skip != 0);
            generateRandom(v, e, selfLoop != 0, sameEdge != 0, dir != 0);
            rebuild();
        } else if (cmd == "complete") {
            int v = 0, skip = 1, dir = 0;
            if (!(iss >> v)) return;
            iss >> skip >> dir;
            skipExtension = (skip != 0);
            generateComplete(v, dir != 0);
            rebuild();
        } else if (cmd == "custom") {
            // skip はグラフ本文より前に置く。本文が複数行なので、
            // 後ろに付けると行の区切りと衝突する。
            int skip = 1;
            iss >> skip;
            skipExtension = (skip != 0);
            if (generateCustom(iss)) rebuild();
        } else {
            layout.is_stable = false;
        }
    }

    // レイアウトの収束計算を進める。描画ループから毎フレーム呼ばれる。
    bool prepare() override {
        if (layout.is_stable) return true;

        if (skipExtension) {
            int frame = 0;
            while (frame < LAYOUT_FRAME_LIMIT && !layout.update(graph.get())) frame++;
            // 時間切れで抜けたときだけ強制パッキングする。
            // 収束して抜けた場合は layout.update の中で既にパッキング済み。
            if (frame >= LAYOUT_FRAME_LIMIT) layout.forcePack(graph.get());
            return true;
        }
        return layout.update(graph.get());
    }

    // 基底クラスは進めるアルゴリズムを持たない
    bool step() override { return false; }

    emscripten::val getState(emscripten::val params) override {
        emscripten::val state = emscripten::val::object();
        if (!graph) return state;

        state.set("nodes", graph->getNodeView());
        state.set("edges", graph->getEdgeView());
        state.set("nodeCount", graph->nodeCount());
        state.set("edgeCount", graph->edgeCount());
        state.set("maxNodes", MAX_NODES);
        state.set("maxEdges", MAX_EDGES);
        state.set("layoutStable", layout.is_stable);
        state.set("startNodeIndex", graph->startNodeIndex);
        state.set("isDirected", generatedDirected);
        state.set("isAutomaton", false);

        // テキスト表現は要求されたときだけ組み立てる。
        // 描画ループが毎フレーム getState を呼ぶので、常に作ると無駄が大きい。
        if (params.hasOwnProperty("withText") && params["withText"].as<bool>()) {
            state.set("graphText", buildGraphText());
        }
        return state;
    }
};
