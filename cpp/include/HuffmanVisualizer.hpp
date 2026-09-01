#pragma once
#include "GraphVisualizer.hpp"
#include "TreeLayout.hpp"
#include "GraphColors.hpp"
#include <algorithm>
#include <cctype>
#include <map>
#include <sstream>
#include <string>
#include <vector>

// ハフマン木の構築のビジュアライザ。
//
// 1ステップは「重みが最小の2つを選ぶ」と「その2つを繋ぐ」の2手。
// 途中経過は木が複数ある森になるので、TreeLayout が木を横に並べる。
//
// 葉には文字、内部の節点には何も書かない。重みはどの節点にも脇に出す。
// 枝の 0 / 1 はそのまま符号のビットで、左右の並び順も兼ねる。
class HuffmanVisualizer : public GraphVisualizer {
public:
    // 葉が k 個なら節点は 2k-1 個になる。頂点数の上限から逆算した葉の上限。
    static constexpr int MAX_LEAVES = (MAX_NODES + 1) / 2;

private:
    // 入力から数えた (文字, 出現回数)。出現回数の小さい順、同じなら文字の順。
    std::vector<std::pair<char, int>> counts;

    // 節点ごとの表示名。葉は文字、内部の節点は空。
    std::vector<std::string> labels;

    struct RunState {
        std::vector<int> roots; // 今の森の根
        int a = -1, b = -1;     // これから繋ぐ2つ。-1 なら次に選ぶ
        int lastMerged = -1;
        bool finished = false;
    };

    RunState st;

    int stepCount = 0;
    bool replaying = false;

    void relayout() {
        if (!replaying) rebuildLayout();
    }

    float nodeValue(int i) const {
        return graph->nodeData[(std::size_t)i * GraphData::NODE_STRIDE + 2];
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

        if (st.lastMerged >= 0) {
            graph->setNodeColor(st.lastMerged, NODE_PATH);
            for (int i = 0; i < graph->edgeCount(); i++) {
                if (graph->edgeFrom(i) == st.lastMerged) graph->setEdgeColor(i, EDGE_ACTIVE);
            }
        }
        // これから繋ぐ2つ。左に来る方を「処理中」、右を「候補」で塗り分ける
        if (st.a >= 0) graph->setNodeColor(st.a, NODE_VISITING);
        if (st.b >= 0) graph->setNodeColor(st.b, NODE_FRONTIER);
    }

    void clearForest() {
        graph = std::make_unique<GraphData>(MAX_NODES, MAX_NODES);
        graph->startNodeIndex = -1;
        labels.clear();
    }

    int addNode(int weight, const std::string& label, float x, float y) {
        int idx = graph->nodeCount();
        graph->setNode(idx, x, y, (float)weight, 0);
        labels.push_back(label);
        return idx;
    }

    void resetRun() {
        clearForest();
        st = RunState{};
        stepCount = 0;

        // 葉を並べる。ここは1手ずつ見せるほどのものではないので最初から出す。
        for (const auto& c : counts) {
            int idx = addNode(c.second, std::string(1, c.first), 0.0f, 0.0f);
            st.roots.push_back(idx);
        }
        relayout();
        syncVisuals();
    }

    // 入力から文字の出現回数を数える。空白は数えない。
    void setTextFrom(const std::string& text) {
        std::map<char, int> freq;
        for (char c : text) {
            if (std::isspace((unsigned char)c)) continue;
            freq[c]++;
        }

        counts.assign(freq.begin(), freq.end());
        // 重みの小さい順。同じなら文字の順にして、選ぶ順を決まったものにする
        std::stable_sort(counts.begin(), counts.end(),
                         [](const std::pair<char, int>& x, const std::pair<char, int>& y) {
                             if (x.second != y.second) return x.second < y.second;
                             return x.first < y.first;
                         });
        if ((int)counts.size() > MAX_LEAVES) counts.resize(MAX_LEAVES);
        resetRun();
    }

    // 重みが最小の2つ。同じ重みなら先にできた方を選び、結果を決まったものにする。
    void pickTwoSmallest(int& first, int& second) const {
        std::vector<int> sorted = st.roots;
        std::stable_sort(sorted.begin(), sorted.end(), [&](int x, int y) {
            float wx = nodeValue(x), wy = nodeValue(y);
            if (wx != wy) return wx < wy;
            return x < y;
        });
        first = sorted[0];
        second = sorted[1];
    }

protected:
    const char* labelMode() const override { return "text"; }
    bool usesSymbols() const override { return true; } // 辺の3列目は 0 / 1

    bool handleCommand(const std::string& source, const std::string& input) override {
        if (source == "setText")  { setTextFrom(input); return true; }
        if (source == "resetRun") { resetRun(); return true; }
        return false;
    }

public:
    HuffmanVisualizer() {
        // 基底のコンストラクタが一般グラフを作っているので、木用に置き換える
        layout = std::make_unique<TreeLayout>();
        weighted = false;
        hasNodeWeights = true;     // 重みを節点の脇に出す
        generatedDirected = false; // 木に矢印は要らない
        skipExtension = false;     // 枝が伸びる様子を見せたいので収束を飛ばさない
        setTextFrom("abracadabra");
    }

    bool step() override {
        if (st.finished) return false;

        // 根が1つになったら木が完成している
        if ((int)st.roots.size() <= 1) {
            st.finished = true;
            st.a = st.b = -1;
            if (!st.roots.empty()) graph->startNodeIndex = st.roots.front();
            syncVisuals();
            return false;
        }

        stepCount++;

        // 1手目: 重みが最小の2つを選ぶ
        if (st.a < 0) {
            pickTwoSmallest(st.a, st.b);
            st.lastMerged = -1;
            syncVisuals();
            return true;
        }

        // 2手目: 選んだ2つを新しい節点の下に繋ぐ
        std::size_t oa = (std::size_t)st.a * GraphData::NODE_STRIDE;
        std::size_t ob = (std::size_t)st.b * GraphData::NODE_STRIDE;
        float mx = (graph->nodeData[oa] + graph->nodeData[ob]) / 2.0f;
        float my = (graph->nodeData[oa + 1] + graph->nodeData[ob + 1]) / 2.0f;

        int weight = (int)(nodeValue(st.a) + nodeValue(st.b));
        int parent = addNode(weight, "", mx, my);

        // 枝の 0 / 1 は符号のビット。TreeLayout の左右の並び順も兼ねる
        graph->addEdge((float)parent, (float)st.a, (float)'0', 0);
        graph->addEdge((float)parent, (float)st.b, (float)'1', 0);

        st.roots.erase(std::remove(st.roots.begin(), st.roots.end(), st.a), st.roots.end());
        st.roots.erase(std::remove(st.roots.begin(), st.roots.end(), st.b), st.roots.end());
        st.roots.push_back(parent);

        st.lastMerged = parent;
        st.a = st.b = -1;
        graph->startNodeIndex = (st.roots.size() == 1) ? parent : -1;

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

        emscripten::val ls = emscripten::val::array();
        for (const std::string& l : labels) ls.call<void>("push", l);
        state.set("nodeLabels", ls);

        emscripten::val cs = emscripten::val::array();
        for (const auto& c : counts) {
            emscripten::val one = emscripten::val::object();
            one.set("ch", std::string(1, c.first));
            one.set("count", c.second);
            cs.call<void>("push", one);
        }
        state.set("counts", cs);

        state.set("insertedCount", graph ? graph->nodeCount() : 0);
        state.set("rootCount", (int)st.roots.size());
        state.set("selectedA", st.a);
        state.set("selectedB", st.b);
        state.set("finished", st.finished);
        state.set("canStepBack", stepCount > 0);
        state.set("maxLeaves", MAX_LEAVES);
        return state;
    }
};
