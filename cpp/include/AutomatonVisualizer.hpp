#pragma once
#include "GraphVisualizer.hpp"
#include "GraphColors.hpp"
#include <algorithm>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// 決定性有限オートマトン (DFA) のビジュアライザ。
//
// オートマトンは有向グラフしか存在しないので、一般グラフのクラスを継承して
// 「常に有向」「初期状態」「受理状態」「遷移記号」「入力の消費」を足す。
// 初期状態・受理状態・遷移関数はアルゴリズムの論理なので JS 側ではなく C++ 側が持つ。
//
// 遷移記号は辺の3列目 (GraphData の weight 欄) に文字コードで入れる。
// オートマトンに重みは無いので、その欄が空いている。
// 同じ頂点対に複数の遷移があっても、描画側が曲線で振り分けるので潰れない。
//
// NFA は現在状態が集合になり、ε-閉包も要るので別のページに分ける。
class AutomatonVisualizer : public GraphVisualizer {
public:
    // アルファベットの上限。これ以上増やすと辺が多すぎて図として読めない。
    static constexpr int MAX_ALPHABET = 8;
    // 入力の上限。履歴を1手ぶんずつ持つので、伸ばすほどメモリを食う。
    static constexpr int MAX_INPUT = 200;

private:
    std::set<int> acceptingStates;

    // 実際に使われている遷移記号。辺から集めるので、テキスト入力でも生成でも同じ。
    std::string alphabet;

    // delta[状態][記号の番号] = 次の状態。-1 は遷移が無い。
    std::vector<std::vector<int>> delta;
    // 同じ位置の辺のインデックス。色を塗るために持つ。
    std::vector<std::vector<int>> deltaEdge;

    // 同じ状態・同じ記号の遷移が2本以上ある。DFA の前提を外れるので警告する。
    bool hasNondeterminism = false;

    std::string inputStr;

    struct RunState {
        int current = -1;
        int pos = 0;                // 入力を何文字目まで消費したか
        std::vector<int> pathEdges; // 通った辺の順。色付けと「直前の遷移」に使う
        bool finished = false;
        bool accepted = false;
        bool stuck = false;         // 遷移が定義されていなくて止まった
    };

    RunState st;
    std::vector<RunState> history;

    int symbolIndex(char c) const {
        std::size_t at = alphabet.find(c);
        return at == std::string::npos ? -1 : (int)at;
    }

    static char edgeSymbol(const GraphData* g, int i) {
        return (char)(int)g->edgeData[i * GraphData::EDGE_STRIDE + 2];
    }

    // 辺から遷移関数を組み直す。TraversalVisualizer が隣接リストを作るのと同じ位置づけ。
    void buildTransitions() {
        alphabet.clear();
        delta.clear();
        deltaEdge.clear();
        hasNondeterminism = false;
        if (!graph) return;

        int n = graph->nodeCount(), m = graph->edgeCount();
        for (int i = 0; i < m; i++) {
            char c = edgeSymbol(graph.get(), i);
            if (alphabet.find(c) == std::string::npos) alphabet.push_back(c);
        }
        std::sort(alphabet.begin(), alphabet.end());

        int k = (int)alphabet.size();
        delta.assign(n, std::vector<int>(k, -1));
        deltaEdge.assign(n, std::vector<int>(k, -1));

        for (int i = 0; i < m; i++) {
            int from = graph->edgeFrom(i), to = graph->edgeTo(i);
            if (from < 0 || from >= n || to < 0 || to >= n) continue;
            int s = symbolIndex(edgeSymbol(graph.get(), i));
            if (s < 0) continue;
            if (delta[from][s] >= 0) {
                // 決定性を外れている。最初の1本を使い、UI で警告する
                hasNondeterminism = true;
                continue;
            }
            delta[from][s] = to;
            deltaEdge[from][s] = i;
        }
    }

    void resetRun() {
        history.clear();
        st = RunState{};
        st.current = graph ? graph->startNodeIndex : -1;
        syncVisuals();
    }

    // 色は毎回状態から作り直す。差分で塗ると戻したときに前の色が残る。
    void syncVisuals() {
        if (!graph) return;
        graph->resetColors();

        int n = graph->nodeCount();
        auto paintNode = [&](int i, int color) {
            if (i >= 0 && i < n) graph->setNodeColor(i, color);
        };

        for (int e : st.pathEdges) {
            graph->setEdgeColor(e, EDGE_VISITED);
            int to = graph->edgeTo(e);
            paintNode(graph->edgeFrom(e), NODE_VISITED);
            paintNode(to, NODE_VISITED);
        }
        if (!st.pathEdges.empty()) graph->setEdgeColor(st.pathEdges.back(), EDGE_ACTIVE);

        paintNode(graph->startNodeIndex, NODE_START);
        // 現在地は最後に塗る。始点と重なっていても「今どこか」の方を見せたい。
        paintNode(st.current, st.finished && st.accepted ? NODE_PATH : NODE_VISITING);
    }

    // 各状態から各記号へ遷移を1本ずつ張る。決定性と全域性が構成から保証される。
    void generateDfa(int v, const std::string& alpha) {
        v = std::clamp(v, 1, MAX_NODES);

        std::string a;
        for (char c : alpha) {
            if (std::isspace((unsigned char)c)) continue;
            if (a.find(c) == std::string::npos) a.push_back(c);
            if ((int)a.size() >= MAX_ALPHABET) break;
        }
        if (a.empty()) a = "ab";
        while (v * (int)a.size() > MAX_EDGES) a.pop_back();

        weighted = false;
        hasNodeWeights = false;
        generatedDirected = true;
        graph = std::make_unique<GraphData>(v, v * (int)a.size());

        for (int i = 0; i < v; i++) scatterNode(i);
        for (int i = 0; i < v; i++)
            for (char c : a)
                graph->addEdge((float)i, (float)randInt(v), (float)(unsigned char)c, 0);

        graph->startNodeIndex = 0;
    }

protected:
    bool forceDirected() const override { return true; }
    bool usesSymbols() const override { return true; }

    // グラフが差し替わったら、範囲外になった状態指定を捨てて遷移を組み直す
    void onGraphChanged() override {
        int n = graph ? graph->nodeCount() : 0;
        if (graph && (graph->startNodeIndex < 0 || graph->startNodeIndex >= n)) {
            graph->startNodeIndex = -1;
        }
        for (auto it = acceptingStates.begin(); it != acceptingStates.end(); ) {
            if (*it < 0 || *it >= n) it = acceptingStates.erase(it);
            else ++it;
        }
        buildTransitions();
        resetRun();
    }

    bool handleCommand(const std::string& source, const std::string& input) override {
        if (source == "setStartNode") {
            int idx = -1;
            std::istringstream iss(input);
            iss >> idx;
            if (graph) {
                graph->startNodeIndex =
                    (idx >= 0 && idx < graph->nodeCount()) ? idx : -1;
            }
            resetRun();
            return true;
        }

        if (source == "setAccepting") {
            // "1, 2, 3" のようなカンマ区切りも受け付ける
            std::string normalized = input;
            for (char& c : normalized) if (c == ',') c = ' ';

            acceptingStates.clear();
            std::istringstream iss(normalized);
            int idx;
            int n = graph ? graph->nodeCount() : 0;
            while (iss >> idx) {
                if (idx >= 0 && idx < n) acceptingStates.insert(idx);
            }
            // 受理状態が変われば判定も変わる
            resetRun();
            return true;
        }

        if (source == "setInput") {
            inputStr.clear();
            for (char c : input) {
                if (std::isspace((unsigned char)c)) continue;
                inputStr.push_back(c);
                if ((int)inputStr.size() >= MAX_INPUT) break;
            }
            resetRun();
            return true;
        }

        if (source == "resetRun") {
            resetRun();
            return true;
        }

        if (source == "genRandom") {
            int v = 4;
            std::string alpha;
            std::istringstream iss(input);
            iss >> v >> alpha;
            generateDfa(v, alpha);
            rebuild();
            return true;
        }

        return false;
    }

public:
    AutomatonVisualizer() {
        generateDfa(4, "ab");
        rebuildLayout(); // コンストラクタなので仮想フックは呼ばない
        buildTransitions();
        resetRun();
    }

    // 入力を1文字消費して遷移する。
    bool step() override {
        if (st.finished) return false;

        history.push_back(st); // どの分岐でも1手ぶん戻せるようにする

        if (st.current < 0) {
            // 初期状態が指定されていない
            st.stuck = true;
            st.finished = true;
            syncVisuals();
            return false;
        }

        if (st.pos >= (int)inputStr.size()) {
            // 読み切った。今いる状態が受理状態かどうかで決まる
            st.finished = true;
            st.accepted = acceptingStates.count(st.current) > 0;
            syncVisuals();
            return false;
        }

        int s = symbolIndex(inputStr[st.pos]);
        int next = (s < 0) ? -1 : delta[st.current][s];
        if (next < 0) {
            // アルファベットに無い記号か、その記号の遷移が定義されていない
            st.stuck = true;
            st.finished = true;
            syncVisuals();
            return false;
        }

        st.pathEdges.push_back(deltaEdge[st.current][s]);
        st.current = next;
        st.pos++;
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
        state.set("isAutomaton", true);

        emscripten::val accepting = emscripten::val::array();
        for (int idx : acceptingStates) accepting.call<void>("push", idx);
        state.set("acceptingStates", accepting);

        state.set("maxAlphabet", MAX_ALPHABET);
        state.set("maxInput", MAX_INPUT);
        state.set("alphabet", alphabet);
        state.set("inputString", inputStr);
        state.set("inputPos", st.pos);
        state.set("currentState", st.current);
        state.set("finished", st.finished);
        state.set("accepted", st.accepted);
        state.set("stuck", st.stuck);
        state.set("hasNondeterminism", hasNondeterminism);
        state.set("canStepBack", !history.empty());

        return state;
    }
};
