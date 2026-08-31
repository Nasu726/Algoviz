#pragma once
#include "IVisualizer.hpp"
#include "GraphData.hpp"
#include "ILayout.hpp"
#include "GeneralGraphLayout.hpp"
#include "GraphColors.hpp"
#include <emscripten/val.h>
#include <memory>
#include <sstream>
#include <string>
#include <random>
#include <algorithm>
#include <vector>
#include <set>

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
    //
    // 50 にしているのは、100 頂点の探索を1手ずつ追える人がいないため。
    // 図として読める範囲に合わせてある。副次的に、初期配置の
    // ヤコビ法 (GeneralGraphLayout の jacobiMethod、実質 O(n^4)) のコストが
    // 100 頂点のときの 16 分の1 になる。
    static constexpr int MAX_NODES = 50;
    static constexpr int MAX_EDGES = 1000;

    // レイアウト収束を諦めるまでのフレーム数。
    // WASM はメインスレッドで同期実行されるので、上限が無いとタブが固まる。
    static constexpr int LAYOUT_FRAME_LIMIT = 3000;

protected:
    std::unique_ptr<GraphData> graph;

    // 配置の決め方は差し替えられる (ILayout)。木を足すときは派生クラスの
    // コンストラクタで別の実装に入れ替える。
    // 仮想ファクトリにしないのは、rebuildLayout() をコンストラクタから呼ぶため。
    // コンストラクタ中の仮想呼び出しは派生クラスへ届かない。
    std::unique_ptr<ILayout> layout = std::make_unique<GeneralGraphLayout>();
    bool skipExtension = true;
    bool generatedDirected = false;
    // 重み付きグラフか。重み無しなら重みを振らず、テキストにも重み列を出さない。
    // ダイクストラが辺長として 1 を使うのは内部の話で、利用者の目には出さない。
    bool weighted = false;

    // テキスト入力で頂点の重みを受け取るか。
    // 「V E」の次の行を辺とみなすか重みの並びとみなすかは、
    // 行の形からは決められないので UI からの指定に従う。
    bool hasNodeWeights = false;

    // グラフを作り直すたびに増える。JS 側が「別のグラフになった」ことを
    // 検出してカメラを合わせ直すのに使う。
    int generation = 0;

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

    // 辺の3列目の意味。既定は重み、オートマトンは遷移記号 (1文字)。
    // GraphData の辺は [from, to, weight, colorId] で、オートマトンに重みは
    // 無いので3列目が空いている。記号の文字コードをそこに入れる。
    virtual bool usesSymbols() const { return false; }

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
        layout->init(graph.get(), adjUndirected);
        layout->invalidate();
        generation++;
    }

    // グラフを差し替えたあとに呼ぶ。派生クラスへの通知まで行う。
    void rebuild() {
        rebuildLayout();
        onGraphChanged();
    }

    // ==========================================
    // グラフ生成
    // ==========================================

    // 重み無しグラフの辺長は 1。表示にも直列化にも出さないので、
    // 利用者から見れば「重みが無い」ままになる。
    float newEdgeWeight() { return weighted ? (float)(randInt(99) + 1) : 1.0f; }

    void addRandomEdge(int from, int to) {
        graph->addEdge((float)from, (float)to, newEdgeWeight(), 0);
    }

    // 多重辺を許さないときに「同じ辺」とみなす組。
    // 無向は向きを無視するので (小, 大) に正規化する。
    std::pair<int, int> edgeKey(int a, int b, bool isDirected) const {
        return isDirected ? std::make_pair(a, b)
                          : std::make_pair(std::min(a, b), std::max(a, b));
    }

    void generateRandom(int v, int e, bool allowSelfLoop, bool allowSameEdge,
                        bool isDirected, bool connected, bool isWeighted) {
        isDirected = isDirected || forceDirected();
        weighted = isWeighted;
        v = std::clamp(v, 0, MAX_NODES);
        e = std::clamp(e, 0, MAX_EDGES);

        // 連結にするには最低でも全域木の V-1 本が要る。足りない指定は引き上げる。
        // 実際の辺数は getState の edgeCount で返るので、UI 側で入力欄に反映できる。
        if (connected && v > 1) e = std::max(e, v - 1);

        generatedDirected = isDirected;
        hasNodeWeights = false;
        graph = std::make_unique<GraphData>(v, e);
        for (int i = 0; i < v; i++) scatterNode(i);

        std::set<std::pair<int, int>> used;

        // 先に全域木を張る。頂点 0 から順に、既に追加済みの頂点を1つ選んで繋ぐ。
        // 有向のときは向きを「既存 -> 新規」に固定するので、頂点 0 を根とする
        // 有向全域木になり、始点 0 からの探索が必ず全頂点へ届く。
        if (connected) {
            for (int i = 1; i < v; i++) {
                int parent = randInt(i);
                addRandomEdge(parent, i);
                used.insert(edgeKey(parent, i, isDirected));
            }
        }

        int remaining = e - (int)used.size();
        if (remaining <= 0) return;

        std::vector<std::pair<int, int>> possible;
        for (int i = 0; i < v; i++) {
            for (int j = (isDirected ? 0 : i); j < v; j++) {
                if (!allowSelfLoop && i == j) continue;
                // 多重辺を許さないなら、全域木で使った組は候補から外す
                if (!allowSameEdge && used.count(edgeKey(i, j, isDirected))) continue;
                possible.push_back({i, j});
            }
        }
        if (possible.empty()) return;

        if (allowSameEdge) {
            for (int i = 0; i < remaining; i++) {
                const auto& p = possible[randInt((int)possible.size())];
                addRandomEdge(p.first, p.second);
            }
        } else {
            std::shuffle(possible.begin(), possible.end(), rng);
            int actual = std::min((int)possible.size(), remaining);
            for (int i = 0; i < actual; i++) {
                addRandomEdge(possible[i].first, possible[i].second);
            }
        }
    }

    void generateComplete(int v, bool isDirected, bool isWeighted) {
        isDirected = isDirected || forceDirected();
        weighted = isWeighted;
        v = std::clamp(v, 0, MAX_NODES);
        generatedDirected = isDirected;
        hasNodeWeights = false;
        int e = isDirected ? v * (v - 1) : v * (v - 1) / 2;
        graph = std::make_unique<GraphData>(v, std::min(e, MAX_EDGES));
        for (int i = 0; i < v; i++) scatterNode(i);
        for (int i = 0; i < v; i++) {
            for (int j = (isDirected ? 0 : i + 1); j < v; j++) {
                if (i == j) continue;
                addRandomEdge(i, j);
            }
        }
    }

    // "V E" の行に続けて "from to [重み]" が E 行。辺の重みは省略できる。
    // withNodeWeights が真なら、"V E" の直後に頂点の重みが V 個並ぶ行が入る。
    bool generateCustom(std::istringstream& iss, bool isDirected,
                        bool withNodeWeights, bool isWeighted) {
        weighted = isWeighted;
        int v = 0, e = 0;
        if (!(iss >> v >> e)) return false;
        v = std::clamp(v, 0, MAX_NODES);
        e = std::clamp(e, 0, MAX_EDGES);
        generatedDirected = isDirected || forceDirected();
        hasNodeWeights = withNodeWeights;
        graph = std::make_unique<GraphData>(v, e);

        std::string line;
        std::getline(iss, line); // "V E" の行の残りを読み捨てる

        // 頂点の重み。V 個の数値として読み切れたときだけ重み行として扱う。
        // 無条件に1行消費すると、重み行を書いていないテキストで
        // 最初の辺の行が食われて辺が静かに消える。
        std::vector<float> nodeWeights(v, 0.0f);
        std::string pending; // 重み行ではなかった行。辺として読み直す
        if (withNodeWeights && v > 0 && std::getline(iss, line)) {
            std::istringstream ws(line);
            std::vector<float> parsed;
            float w;
            while (ws >> w) parsed.push_back(w);
            if ((int)parsed.size() == v) nodeWeights = parsed;
            else pending = line;
        }
        for (int i = 0; i < v; i++) graph->setNode(i, (float)i, (float)i, nodeWeights[i], 0);

        int added = 0;
        bool more = true;
        while (added < e && more) {
            if (!pending.empty()) { line = pending; pending.clear(); }
            else more = (bool)std::getline(iss, line);
            if (!more) break;

            std::istringstream ls(line);
            int from, to;
            // 3列目が無いときは辺長 1。重み無しグラフでは表示にも出さない。
            float w = 1.0f;
            if (!(ls >> from >> to)) continue; // 空行や不正な行は飛ばす
            if (usesSymbols()) {
                char sym;
                if (!(ls >> sym)) continue; // 記号の無い遷移は遷移になっていない
                w = (float)(unsigned char)sym;
            } else if (weighted) {
                ls >> w;
            }
            if (from < 0 || from >= v || to < 0 || to >= v) continue;
            graph->addEdge((float)from, (float)to, w, 0);
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

    // nodeValues は頂点の重み欄の元の値。探索中はここが暫定距離の表示に
    // 使われることがあるので、テキスト化するときは派生クラスから元の値をもらう。
    virtual const std::vector<float>* originalNodeWeights() const { return nullptr; }

    std::string buildGraphText() const {
        std::ostringstream oss;
        int v = graph->nodeCount(), e = graph->edgeCount();
        oss << v << " " << e << "\n";

        if (hasNodeWeights) {
            const std::vector<float>* w = originalNodeWeights();
            for (int i = 0; i < v; i++) {
                if (i) oss << " ";
                oss << (w && i < (int)w->size() ? (*w)[i]
                                                : graph->nodeData[i * GraphData::NODE_STRIDE + 2]);
            }
            oss << "\n";
        }

        for (int i = 0; i < e; i++) {
            float extra = graph->edgeData[i * GraphData::EDGE_STRIDE + 2];
            oss << graph->edgeFrom(i) << " " << graph->edgeTo(i);
            if (usesSymbols())    oss << " " << (char)(int)extra;
            else if (weighted)    oss << " " << extra;
            oss << "\n";
        }
        return oss.str();
    }

public:
    GraphVisualizer() {
        generateRandom(8, 10, false, false, false, true, false);
        rebuildLayout(); // コンストラクタなので仮想フックは呼ばない
    }

    // source: レイアウトの指向性、または派生クラス向けのコマンド名
    // input : グラフ生成コマンド ("random" / "complete" / "custom")
    void load(const std::string& source, const std::string& input) override {
        if (source == "horizontal")     layout->setPreferHorizontal(true);
        else if (source == "vertical")  layout->setPreferHorizontal(false);
        else if (handleCommand(source, input)) return;

        if (input.empty()) {
            layout->invalidate();
            return;
        }

        std::istringstream iss(input);
        std::string cmd;
        iss >> cmd;

        if (cmd == "random") {
            int v = 0, e = 0, skip = 1, selfLoop = 0, sameEdge = 0, dir = 0, conn = 0, wt = 0;
            if (!(iss >> v >> e)) return;
            iss >> skip >> selfLoop >> sameEdge >> dir >> conn >> wt;
            skipExtension = (skip != 0);
            generateRandom(v, e, selfLoop != 0, sameEdge != 0, dir != 0, conn != 0, wt != 0);
            rebuild();
        } else if (cmd == "complete") {
            int v = 0, skip = 1, dir = 0, wt = 0;
            if (!(iss >> v)) return;
            iss >> skip >> dir >> wt;
            skipExtension = (skip != 0);
            generateComplete(v, dir != 0, wt != 0);
            rebuild();
        } else if (cmd == "custom") {
            // skip と向きはグラフ本文より前に置く。本文が複数行なので、
            // 後ろに付けると行の区切りと衝突する。
            // ヘッダは1行目だけから読む。行をまたいで読むと、引数を省略したときに
            // 本文の数値を引数として食ってしまい、グラフが丸ごと壊れる。
            std::string header;
            std::getline(iss, header);
            std::istringstream hs(header);

            int skip = 1, dir = 0, nodeW = 0, wt = 0;
            hs >> skip >> dir >> nodeW >> wt;
            skipExtension = (skip != 0);
            if (generateCustom(iss, dir != 0, nodeW != 0, wt != 0)) rebuild();
        } else {
            layout->invalidate();
        }
    }

    // レイアウトの収束計算を進める。描画ループから毎フレーム呼ばれる。
    bool prepare() override {
        if (layout->isStable()) return true;

        if (skipExtension) {
            int frame = 0;
            while (frame < LAYOUT_FRAME_LIMIT && !layout->update(graph.get())) frame++;
            // 時間切れで抜けたときだけ打ち切る。
            // 収束して抜けた場合は update の中で最終処理まで済んでいる。
            if (frame >= LAYOUT_FRAME_LIMIT) layout->finish(graph.get());
            return true;
        }
        return layout->update(graph.get());
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
        state.set("layoutStable", layout->isStable());
        state.set("generation", generation);
        state.set("startNodeIndex", graph->startNodeIndex);
        state.set("isDirected", generatedDirected);
        state.set("isAutomaton", false);
        state.set("hasNodeWeights", hasNodeWeights);
        state.set("weighted", weighted);

        // テキスト表現は要求されたときだけ組み立てる。
        // 描画ループが毎フレーム getState を呼ぶので、常に作ると無駄が大きい。
        if (params.hasOwnProperty("withText") && params["withText"].as<bool>()) {
            state.set("graphText", buildGraphText());
        }
        return state;
    }
};
