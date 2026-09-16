#pragma once
#include "GraphVisualizer.hpp"
#include "TreeLayout.hpp"
#include "GraphColors.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

// Union-Find (素集合データ構造) のビジュアライザ。
//
// 要素 0〜n-1 を節点、親への辺を持つ森として描く。最初は全部が根。
// 操作は語で書く: `union a b` / `find x`。
//
//   find x    … 親をたどって1つ上がるのが1手。根に着いたら「根が分かった」。
//               そのあと**経路圧縮**: 通った節点を1つずつ根の直下に付け替える (1手ずつ)
//   union a b … find a、find b を上のとおり進めたあと、**小さい木を大きい木の根の
//               下に付ける** (1手、union by size)。根が同じなら「既に同じ集合」(1手)
//
// 経路圧縮で木が平たくなる様子と、大きい木の下に付くので背が伸びない様子が
// 見どころ。どちらも動きを見ないと分からない。
//
// 辺は parent[] から毎手作り直す。GraphData に辺を消す口が無いので、付け替えは
// 全部の辺を作り直す形にする。要素の数は操作に出てくる最大の番号 + 1。
class UnionFindVisualizer : public GraphVisualizer {
public:
    static constexpr int MAX_ELEMENTS = 20;
    static constexpr int MAX_OPS = 20;

private:
    struct Op {
        bool isUnion = false;
        int a = 0, b = 0; // find は a だけ
    };

    std::vector<Op> ops;
    int elements = 2;

    std::vector<int> parent, size;
    int opIndex = 0;

    // 進行中の操作。union は find を2回まわしてから link
    enum Phase { Idle, Climb, Compress, Link };
    Phase phase = Idle;
    int cursor = -1;         // Climb: 今いる節点
    std::vector<int> path;   // Climb で通った節点 (根を除く)
    int compressAt = 0;      // Compress: path の何番目を付け替えるか
    int rootA = -1, rootB = -1;
    bool secondFind = false; // union の2回目の find か

    // 直前の手が何だったか (パネルの文と色に使う)
    int climbedFrom = -1, climbedTo = -1;
    int foundRoot = -1;
    int compressed = -1;     // 根の直下に付け直した節点
    int linkedChild = -1, linkedRoot = -1;
    bool sameSet = false;

    int stepCount = 0;
    bool replaying = false;
    bool finished = false;

    static std::string lowered(const std::string& s) {
        std::string out = s;
        for (char& c : out) c = (char)std::tolower((unsigned char)c);
        return out;
    }

    // "union 1 2 find 5" のような並びを読む。要素の数は最大の番号 + 1
    void setOpsFrom(const std::string& text) {
        ops.clear();
        int most = 1;
        std::istringstream iss(text);
        std::string word;
        while (iss >> word && (int)ops.size() < MAX_OPS) {
            std::string w = lowered(word);
            Op op;
            if (w == "union") {
                if (!(iss >> op.a >> op.b)) break;
                op.isUnion = true;
            } else if (w == "find") {
                if (!(iss >> op.a)) break;
                op.b = op.a;
            } else {
                continue; // 読めない語は飛ばす
            }
            op.a = std::clamp(op.a, 0, MAX_ELEMENTS - 1);
            op.b = std::clamp(op.b, 0, MAX_ELEMENTS - 1);
            most = std::max({most, op.a, op.b});
            ops.push_back(op);
        }
        elements = most + 1;
        resetRun();
    }

    // 8 要素で、union と find を混ぜて作る
    void generateOps(int howMany) {
        howMany = std::clamp(howMany, 1, MAX_OPS - 1);
        const int n = 8;
        std::string text;
        for (int i = 0; i < howMany; i++) {
            if (randInt(10) < 7) {
                int a = randInt(n), b = randInt(n);
                while (b == a) b = randInt(n);
                text += "union " + std::to_string(a) + " " + std::to_string(b) + " ";
            } else {
                text += "find " + std::to_string(randInt(n)) + " ";
            }
        }
        // 全部の要素が出るように、最後の番号を含める
        text += "find " + std::to_string(n - 1) + " ";
        setOpsFrom(text);
    }

    // 親の配列から辺を作り直す。3列目は子の番号で、左右の並びを安定させる
    void rebuildEdges() {
        graph->edgeData.clear();
        for (int i = 0; i < elements; i++) {
            if (parent[i] != i) graph->addEdge((float)parent[i], (float)i, (float)i, 0);
        }
        if (!replaying) rebuildLayout();
    }

    int findEdgeTo(int child) const {
        for (int i = 0; i < graph->edgeCount(); i++) {
            if (graph->edgeTo(i) == child) return i;
        }
        return -1;
    }

    // 色は毎回状態から作り直す
    void syncVisuals() {
        if (!graph) return;
        graph->resetColors();
        if (finished) return;
        for (int p : path) graph->setNodeColor(p, NODE_VISITED); // 通った節点
        if (rootA >= 0) graph->setNodeColor(rootA, NODE_FRONTIER);
        if (rootB >= 0) graph->setNodeColor(rootB, NODE_FRONTIER);
        if (foundRoot >= 0) graph->setNodeColor(foundRoot, NODE_FRONTIER);
        if (cursor >= 0) graph->setNodeColor(cursor, NODE_VISITING);
        if (compressed >= 0) {
            graph->setNodeColor(compressed, NODE_PATH);
            graph->setEdgeColor(findEdgeTo(compressed), EDGE_ACTIVE);
        }
        if (linkedChild >= 0) {
            graph->setNodeColor(linkedChild, NODE_PATH);
            graph->setEdgeColor(findEdgeTo(linkedChild), EDGE_ACTIVE);
        }
    }

    void clearLastMove() {
        climbedFrom = climbedTo = -1;
        foundRoot = -1;
        compressed = -1;
        linkedChild = linkedRoot = -1;
        sameSet = false;
    }

    // find を始める
    void beginFind(int x) {
        phase = Climb;
        cursor = x;
        path.clear();
        compressAt = 0;
    }

    int foundRootOf() const { return secondFind ? rootB : rootA; }

    void resetRun() {
        graph = std::make_unique<GraphData>(MAX_ELEMENTS, MAX_ELEMENTS);
        graph->startNodeIndex = -1;
        parent.assign(elements, 0);
        size.assign(elements, 1);
        for (int i = 0; i < elements; i++) {
            parent[i] = i;
            // 横に並べて始める。配置が動かす
            graph->setNode(i, (float)i * 60.0f, 0.0f, 0.0f, 0);
        }
        opIndex = 0;
        phase = Idle;
        cursor = -1;
        path.clear();
        compressAt = 0;
        rootA = rootB = -1;
        secondFind = false;
        clearLastMove();
        stepCount = 0;
        finished = ops.empty();
        rebuildEdges();
        if (!replaying) layout->finish(graph.get());
        syncVisuals();
    }

    // 1手進める。true なら手が進んだ
    bool advance() {
        if (finished) return false;
        clearLastMove();

        for (;;) {
            switch (phase) {
            case Idle: {
                if (opIndex >= (int)ops.size()) { finished = true; return false; }
                const Op& op = ops[opIndex];
                rootA = rootB = -1;
                secondFind = false;
                beginFind(op.a);
                break;
            }

            case Climb: {
                if (parent[cursor] != cursor) {
                    // 1つ上がる
                    climbedFrom = cursor;
                    path.push_back(cursor);
                    cursor = parent[cursor];
                    climbedTo = cursor;
                    return true;
                }
                // 根に着いた
                foundRoot = cursor;
                if (!secondFind) rootA = cursor; else rootB = cursor;
                phase = Compress;
                compressAt = 0;
                cursor = -1;
                return true;
            }

            case Compress: {
                // 通った節点を根の直下に付け替える。既に直下のものは付け替える
                // ものが無いので飛ばす (見ても何も起きない)
                int root = foundRootOf();
                while (compressAt < (int)path.size() && parent[path[compressAt]] == root) {
                    compressAt++;
                }
                if (compressAt < (int)path.size()) {
                    int node = path[compressAt++];
                    parent[node] = root;
                    compressed = node;
                    rebuildEdges();
                    return true;
                }
                // 圧縮し終えた。union なら2回目の find へ、それから link
                path.clear();
                const Op& op = ops[opIndex];
                if (op.isUnion && !secondFind) {
                    secondFind = true;
                    beginFind(op.b);
                    break;
                }
                if (op.isUnion) { phase = Link; break; }
                // find はここで終わり
                opIndex++;
                phase = Idle;
                rootA = rootB = -1;
                break;
            }

            case Link: {
                if (rootA == rootB) {
                    sameSet = true;
                } else {
                    // 小さい木を大きい木の下に。同じなら a の下に b
                    int big = rootA, small = rootB;
                    if (size[rootB] > size[rootA]) { big = rootB; small = rootA; }
                    parent[small] = big;
                    size[big] += size[small];
                    linkedChild = small;
                    linkedRoot = big;
                    rebuildEdges();
                }
                opIndex++;
                phase = Idle;
                rootA = rootB = -1;
                return true;
            }
            }
        }
    }

    int setCount() const {
        int c = 0;
        for (int i = 0; i < elements; i++) if (parent[i] == i) c++;
        return c;
    }

protected:
    const char* labelMode() const override { return "index"; }

    bool handleCommand(const std::string& source, const std::string& input) override {
        if (source == "setValues") { setOpsFrom(input); return true; }
        if (source == "resetRun")  { resetRun(); return true; }
        if (source == "genRandom") {
            int howMany = 10;
            std::istringstream iss(input);
            iss >> howMany;
            generateOps(howMany);
            return true;
        }
        return false;
    }

public:
    UnionFindVisualizer() {
        layout = std::make_unique<TreeLayout>();
        weighted = false;
        hasNodeWeights = false;
        generatedDirected = false;
        skipExtension = false; // 付け替わる様子を見せたいので収束を飛ばさない
        setOpsFrom("union 0 1 union 2 3 union 1 3 find 0 union 4 5 union 3 5 find 4");
    }

    bool step() override {
        if (!advance()) { syncVisuals(); return false; }
        stepCount++;
        syncVisuals();
        return true;
    }

    // 1手少なく最初から流し直す。辺を消す口が無いので、作り直す方が確実
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

        emscripten::val list = emscripten::val::array();
        for (const Op& op : ops) {
            std::string label = op.isUnion
                ? "union " + std::to_string(op.a) + " " + std::to_string(op.b)
                : "find " + std::to_string(op.a);
            list.call<void>("push", label);
        }
        state.set("ops", list);
        state.set("opIndex", opIndex);
        state.set("maxOps", MAX_OPS);
        state.set("finished", finished);
        state.set("canStepBack", stepCount > 0);

        emscripten::val parents = emscripten::val::array();
        emscripten::val sizes = emscripten::val::array();
        for (int i = 0; i < elements; i++) {
            parents.call<void>("push", parent[i]);
            sizes.call<void>("push", size[i]);
        }
        state.set("parent", parents);
        state.set("size", sizes);
        state.set("elements", elements);
        state.set("setCount", setCount());

        state.set("phase", phase == Idle ? "idle" : phase == Climb ? "climb"
                           : phase == Compress ? "compress" : "link");
        state.set("cursor", cursor);
        state.set("climbedFrom", climbedFrom);
        state.set("climbedTo", climbedTo);
        state.set("foundRoot", foundRoot);
        state.set("compressed", compressed);
        state.set("linkedChild", linkedChild);
        state.set("linkedRoot", linkedRoot);
        state.set("sameSet", sameSet);
        return state;
    }
};
