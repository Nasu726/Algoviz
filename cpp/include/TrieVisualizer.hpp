#pragma once
#include "GraphVisualizer.hpp"
#include "TreeLayout.hpp"
#include "GraphColors.hpp"
#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// trie (接頭辞木) の構築のビジュアライザ。
//
// 1ステップは「1文字進む」。単語を1つ入れるのに、根から文字数ぶんの
// ステップがかかる。共通の接頭辞が1本にまとまっていく様子が見える。
//
// 文字は辺の3列目に文字コードで入れる。TreeLayout はこの欄で子を並べ替えるので、
// 文字コード順 = 辞書順の左右になり、並び順の指定を兼ねられる。
//
// 節点そのものには名前が無い (根からの道がその接頭辞を表す) ので、
// labelMode は none。単語の終わりは受理状態と同じ二重丸で示す。
class TrieVisualizer : public GraphVisualizer {
public:
    static constexpr int MAX_WORDS = 12;
    static constexpr int MAX_WORD_LENGTH = 12;

private:
    std::vector<std::string> words;

    // childOf[節点] = (文字, 行き先) の並び。挿入しながら育てる。
    std::vector<std::vector<std::pair<char, int>>> childOf;
    std::set<int> terminals; // 単語がそこで終わる節点

    struct RunState {
        int pending = 0;  // これから入れる単語が words の何番目か
        int pos     = 0;  // その単語を何文字目まで読んだか
        int cursor  = -1; // 今いる節点。-1 なら次の単語を根から始める前
        int lastCreated = -1;
        std::vector<int> pathNodes;
        std::vector<int> pathEdges;
        bool finished = false;
    };

    RunState st;

    // 進めた手数。stepBack は「最初から1手少なく流し直す」で戻す。
    int stepCount = 0;
    bool replaying = false;

    void relayout() {
        if (!replaying) rebuildLayout();
    }

    int childWith(int node, char c) const {
        for (const auto& e : childOf[node]) if (e.first == c) return e.second;
        return -1;
    }

    int findEdge(int from, int to) const {
        for (int i = 0; i < graph->edgeCount(); i++) {
            if (graph->edgeFrom(i) == from && graph->edgeTo(i) == to) return i;
        }
        return -1;
    }

    // 新しい節点は親の位置から生やす。ランダムな位置に置くと画面の端から飛んでくる。
    int addChild(int parent, char c) {
        int idx = graph->nodeCount();
        std::size_t p = (std::size_t)parent * GraphData::NODE_STRIDE;
        graph->setNode(idx, graph->nodeData[p], graph->nodeData[p + 1], 0, 0);
        // 辺の3列目は文字。TreeLayout がこの値で子を並べるので辞書順になる
        graph->addEdge((float)parent, (float)idx, (float)(unsigned char)c, 0);
        childOf.emplace_back();
        childOf[parent].push_back({c, idx});
        return idx;
    }

    // 色は毎回状態から作り直す。差分で塗ると戻したときに前の色が残る。
    void syncVisuals() {
        if (!graph) return;
        graph->resetColors();

        for (int e : st.pathEdges) graph->setEdgeColor(e, EDGE_VISITED);
        if (!st.pathEdges.empty()) graph->setEdgeColor(st.pathEdges.back(), EDGE_ACTIVE);

        for (int n : st.pathNodes) graph->setNodeColor(n, NODE_VISITED);
        if (st.lastCreated >= 0) graph->setNodeColor(st.lastCreated, NODE_PATH);
        if (st.cursor >= 0) graph->setNodeColor(st.cursor, NODE_VISITING);
    }

    // 空の trie は根だけを持つ
    void clearTrie() {
        graph = std::make_unique<GraphData>(MAX_NODES, MAX_NODES);
        graph->setNode(0, 0.0f, 0.0f, 0, 0);
        graph->startNodeIndex = 0;
        childOf.assign(1, {});
        terminals.clear();
    }

    void resetRun() {
        clearTrie();
        st = RunState{};
        stepCount = 0;
        relayout();
        syncVisuals();
    }

    void setWordsFrom(const std::string& text) {
        words.clear();
        std::istringstream iss(text);
        std::string w;
        while (iss >> w && (int)words.size() < MAX_WORDS) {
            std::string cleaned;
            for (char c : w) {
                if (std::isspace((unsigned char)c)) continue;
                cleaned.push_back(c);
                if ((int)cleaned.size() >= MAX_WORD_LENGTH) break;
            }
            if (!cleaned.empty()) words.push_back(cleaned);
        }
        resetRun();
    }

protected:
    const char* labelMode() const override { return "none"; }
    bool usesSymbols() const override { return true; }

    bool handleCommand(const std::string& source, const std::string& input) override {
        if (source == "setWords") { setWordsFrom(input); return true; }
        if (source == "resetRun") { resetRun(); return true; }
        return false;
    }

public:
    TrieVisualizer() {
        // 基底のコンストラクタが一般グラフを作っているので、木用に置き換える
        layout = std::make_unique<TreeLayout>();
        weighted = false;
        hasNodeWeights = false;
        generatedDirected = false; // 木に矢印は要らない
        skipExtension = false;     // 枝が伸びる様子を見せたいので収束を飛ばさない
        setWordsFrom("to tea ten ted i in inn");
    }

    // 1文字進む。文字の行き先が無ければ、そこで新しい節点を作る。
    bool step() override {
        if (st.finished) return false;

        // 次の単語を根から始める
        if (st.cursor < 0) {
            if (st.pending >= (int)words.size()) {
                st.finished = true;
                st.lastCreated = -1;
                syncVisuals();
                return false;
            }
            stepCount++;
            st.cursor = graph->startNodeIndex;
            st.pos = 0;
            st.lastCreated = -1;
            st.pathNodes.assign(1, st.cursor);
            st.pathEdges.clear();
            syncVisuals();
            return true;
        }

        stepCount++;
        const std::string& word = words[st.pending];

        // 読み切った。ここが単語の終わり
        if (st.pos >= (int)word.size()) {
            terminals.insert(st.cursor);
            st.pending++;
            st.cursor = -1;
            st.lastCreated = -1;
            syncVisuals();
            return true;
        }

        char c = word[st.pos];
        int next = childWith(st.cursor, c);

        if (next < 0) {
            // 節点の上限に達したら、これ以上は伸ばせない
            if (graph->nodeCount() >= MAX_NODES) {
                st.finished = true;
                syncVisuals();
                return false;
            }
            next = addChild(st.cursor, c);
            st.lastCreated = next;
            relayout();
        } else {
            st.lastCreated = -1;
        }

        st.pathEdges.push_back(findEdge(st.cursor, next));
        st.cursor = next;
        st.pathNodes.push_back(next);
        st.pos++;
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

        emscripten::val ws = emscripten::val::array();
        for (const std::string& w : words) ws.call<void>("push", w);
        state.set("words", ws);

        // 単語の終わりは受理状態と同じ二重丸で示す
        emscripten::val accepting = emscripten::val::array();
        for (int idx : terminals) accepting.call<void>("push", idx);
        state.set("acceptingStates", accepting);

        state.set("pending", st.pending);
        state.set("cursor", st.cursor);
        state.set("finished", st.finished);
        state.set("canStepBack", stepCount > 0);
        state.set("insertedCount", graph ? graph->nodeCount() : 0);
        state.set("maxWords", MAX_WORDS);

        // 根から今いる節点までが、そこまでに読んだ接頭辞
        std::string prefix;
        if (st.pending < (int)words.size()) prefix = words[st.pending].substr(0, st.pos);
        state.set("prefix", prefix);
        return state;
    }
};
