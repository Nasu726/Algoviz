#pragma once
#include "GraphVisualizer.hpp"
#include "TreeLayout.hpp"
#include "GraphColors.hpp"
#include <algorithm>
#include <cctype>
#include <climits>
#include <sstream>
#include <string>
#include <vector>

// セグメント木のビジュアライザ。
//
// 配列 a[0..n) の上に再帰で木を作る。節点は半開区間 [l, r) を受け持ち、
// 子は [l, mid) と [mid, r)。n は 2 の冪でなくてよい (葉の深さが揃わないだけ)。
//
//   組み立て   … 内部の節点を後順に1つずつ計算する (左の子 ⊕ 右の子)。1手で1節点
//   update i x … 1手目で葉 i を書き換え、以後1手ごとに親へ上がって計算し直す
//   query l r  … 1手で節点を1つ見る。全部入る → 答えに使って降りない /
//                外れる → 何もしない / 一部かかる → 子へ降りる (左から)
//
// 集約 ⊕ は和 / 最小 / 最大から選ぶ。どれも正しいセグメント木で、分類では決まらない。
// 木の形は配列の長さだけで決まるので、節点も辺も最初に全部作る。動くのは値だけ。
class SegmentTreeVisualizer : public GraphVisualizer {
public:
    static constexpr int MAX_LEAVES = 16;
    static constexpr int MAX_OPS = 10;
    static constexpr int MAX_VALUE = 99;

    enum Monoid { Sum, Min, Max };

private:
    struct Node {
        int l = 0, r = 0;
        int left = -1, right = -1, parent = -1;
        int edge = -1;     // 親からこの節点への辺
        int value = 0;
        bool computed = false;
    };

    struct Op {
        bool isUpdate = false;
        int a = 0, b = 0; // update i x / query l r
    };

    std::vector<int> values;
    std::vector<Op> ops;
    Monoid monoid = Sum;

    std::vector<Node> nodes;
    std::vector<int> leafOf;     // 添字 → 葉の節点
    std::vector<int> buildOrder; // 内部の節点の後順

    enum Phase { Build, Idle, Update, Query, Done };
    Phase phase = Build;
    int buildAt = 0;
    int opIndex = 0;

    // update: 次に計算し直す節点。leafPending なら次の手は葉の書き換え
    bool leafPending = false;
    int cursor = -1;
    // query: これから見る節点 (末尾から取る) と、答えに使った節点
    std::vector<int> stack;
    std::vector<int> taken;
    int answer = 0;
    int ql = 0, qr = 0;

    // 色の元。操作が変わるたびに消す
    std::vector<char> touched; // 組み立て・update で計算した
    enum Class { None, In, Out, Partial };
    std::vector<Class> classOf; // query で見た結果

    // 直前の手
    int lastNode = -1;
    std::string lastKind;

    int stepCount = 0;

    int identity() const {
        return monoid == Sum ? 0 : monoid == Min ? INT_MAX : INT_MIN;
    }

    int combine(int a, int b) const {
        return monoid == Sum ? a + b : monoid == Min ? std::min(a, b) : std::max(a, b);
    }

    int n() const { return (int)values.size(); }

    static std::string lowered(const std::string& s) {
        std::string out = s;
        for (char& c : out) c = (char)std::tolower((unsigned char)c);
        return out;
    }

    // 前順に節点を作る。番号 0 が根。辺の3列目で左 (0) / 右 (1) を決める
    int create(int l, int r, int parent, float side) {
        int id = (int)nodes.size();
        Node node;
        node.l = l;
        node.r = r;
        node.parent = parent;
        nodes.push_back(node);
        graph->setNode(id, (float)l * 60.0f, 0.0f, 0.0f, 0);
        if (parent >= 0) {
            nodes[id].edge = graph->edgeCount();
            graph->addEdge((float)parent, (float)id, side, 0);
        }

        if (r - l == 1) {
            nodes[id].value = values[l];
            nodes[id].computed = true;
            leafOf[l] = id;
            return id;
        }
        int mid = (l + r) / 2;
        int left = create(l, mid, id, 0.0f);
        int right = create(mid, r, id, 1.0f);
        nodes[id].left = left;
        nodes[id].right = right;
        buildOrder.push_back(id); // 子を作り終えてから並べるので後順
        return id;
    }

    void recompute(int u) {
        nodes[u].value = combine(nodes[nodes[u].left].value, nodes[nodes[u].right].value);
        nodes[u].computed = true;
    }

    void resetRun() {
        graph = std::make_unique<GraphData>(2 * MAX_LEAVES, 2 * MAX_LEAVES);
        graph->startNodeIndex = -1;
        nodes.clear();
        buildOrder.clear();
        leafOf.assign(n(), -1);
        if (n() > 0) create(0, n(), -1, 0.0f);

        buildAt = 0;
        opIndex = 0;
        leafPending = false;
        cursor = -1;
        stack.clear();
        taken.clear();
        answer = identity();
        ql = qr = 0;
        touched.assign(nodes.size(), 0);
        classOf.assign(nodes.size(), None);
        lastNode = -1;
        lastKind.clear();
        stepCount = 0;
        phase = nodes.empty() ? Done : Build;
        // 形は最初から決まっているので、寄せずにその場へ置く
        rebuildLayout();
        layout->finish(graph.get());
        syncVisuals();
    }

    void setValuesFrom(const std::string& text) {
        values.clear();
        std::istringstream iss(text);
        int v;
        while (iss >> v && n() < MAX_LEAVES) values.push_back(std::clamp(v, -MAX_VALUE, MAX_VALUE));
        resetRun();
    }

    // "update 3 9 query 1 6" のような並びを読む。番号は動かすときに端へ寄せる
    // (配列の長さを後から変えても読み直さずに済む)
    void setOpsFrom(const std::string& text) {
        ops.clear();
        std::istringstream iss(text);
        std::string word;
        while (iss >> word && (int)ops.size() < MAX_OPS) {
            std::string w = lowered(word);
            Op op;
            if (w == "update") op.isUpdate = true;
            else if (w != "query") continue; // 読めない語は飛ばす
            if (!(iss >> op.a >> op.b)) break;
            if (op.isUpdate) op.b = std::clamp(op.b, -MAX_VALUE, MAX_VALUE);
            ops.push_back(op);
        }
        resetRun();
    }

    // 1〜9 の値と、query と update を交互に並べた操作を作る
    void generate(int count) {
        count = std::clamp(count, 1, MAX_LEAVES);
        values.clear();
        for (int i = 0; i < count; i++) values.push_back(randInt(9) + 1);

        std::string text;
        for (int k = 0; k < 5; k++) {
            if (k % 2 == 0) {
                int l = randInt(count), r = l + 1 + randInt(count - l);
                text += "query " + std::to_string(l) + " " + std::to_string(r) + " ";
            } else {
                text += "update " + std::to_string(randInt(count)) + " " +
                        std::to_string(randInt(9) + 1) + " ";
            }
        }
        setOpsFrom(text);
    }

    // 操作を始める。前の操作の色はここで消す
    void beginOp(const Op& op) {
        std::fill(touched.begin(), touched.end(), 0);
        std::fill(classOf.begin(), classOf.end(), None);
        if (op.isUpdate) {
            phase = Update;
            leafPending = true;
            cursor = leafOf[std::clamp(op.a, 0, n() - 1)];
        } else {
            phase = Query;
            ql = std::clamp(op.a, 0, n());
            qr = std::clamp(op.b, 0, n());
            stack.assign(1, 0);
            taken.clear();
            answer = identity();
        }
    }

    bool opComplete() const {
        if (phase == Build) return buildAt >= (int)buildOrder.size();
        if (phase == Update) return !leafPending && cursor < 0;
        if (phase == Query) return stack.empty();
        return true;
    }

    // 1手進める。true なら手が進んだ
    bool advance() {
        lastNode = -1;
        lastKind.clear();
        if (phase == Done) return false;

        // 前の段階を終えていれば次の操作へ。終えた操作の色は、次の手まで残す
        if (opComplete()) {
            if (phase == Update || phase == Query) opIndex++;
            phase = Idle;
            if (opIndex >= (int)ops.size()) { phase = Done; return false; }
            beginOp(ops[opIndex]);
        }

        switch (phase) {
        case Build: {
            int u = buildOrder[buildAt++];
            recompute(u);
            touched[u] = 1;
            lastNode = u;
            lastKind = "build";
            return true;
        }
        case Update: {
            int u = cursor;
            if (leafPending) {
                leafPending = false;
                nodes[u].value = ops[opIndex].b;
                lastKind = "leaf";
            } else {
                recompute(u);
                lastKind = "recompute";
            }
            touched[u] = 1;
            lastNode = u;
            cursor = nodes[u].parent;
            return true;
        }
        case Query: {
            int u = stack.back();
            stack.pop_back();
            const Node& node = nodes[u];
            lastNode = u;
            if (ql >= qr || node.r <= ql || qr <= node.l) {
                classOf[u] = Out;
                lastKind = "out";
            } else if (ql <= node.l && node.r <= qr) {
                classOf[u] = In;
                taken.push_back(u);
                answer = combine(answer, node.value);
                lastKind = "in";
            } else {
                // 葉は必ず全部入るか外れるので、ここに来るのは内部の節点だけ
                classOf[u] = Partial;
                stack.push_back(node.right); // 左から見るので右を先に積む
                stack.push_back(node.left);
                lastKind = "partial";
            }
            return true;
        }
        default:
            return false;
        }
    }

    // 色は毎回状態から作り直す
    void syncVisuals() {
        if (!graph) return;
        graph->resetColors();
        for (int u = 0; u < (int)nodes.size(); u++) {
            if (touched[u] || classOf[u] == In) graph->setNodeColor(u, NODE_START);
            if (classOf[u] == Partial) graph->setNodeColor(u, NODE_FRONTIER);
            if (classOf[u] == Out) graph->setNodeColor(u, NODE_VISITED);
        }
        if (lastNode < 0) return;
        const Node& node = nodes[lastNode];
        // 計算した手は読んだ子への2本、query は降りてきた1本
        if (lastKind == "build" || lastKind == "recompute") {
            graph->setEdgeColor(nodes[node.left].edge, EDGE_ACTIVE);
            graph->setEdgeColor(nodes[node.right].edge, EDGE_ACTIVE);
        } else if (phase == Query && node.edge >= 0) {
            graph->setEdgeColor(node.edge, EDGE_ACTIVE);
        }
    }

    std::string opLabel(const Op& op) const {
        return std::string(op.isUpdate ? "update " : "query ") +
               std::to_string(op.a) + " " + std::to_string(op.b);
    }

protected:
    const char* labelMode() const override { return "text"; }

    bool handleCommand(const std::string& source, const std::string& input) override {
        if (source == "setValues") { setValuesFrom(input); return true; }
        if (source == "setOps")    { setOpsFrom(input); return true; }
        if (source == "resetRun")  { resetRun(); return true; }
        if (source == "setMonoid") {
            std::string w = lowered(input);
            monoid = w == "min" ? Min : w == "max" ? Max : Sum;
            resetRun(); // 集約が変われば木の値も変わる
            return true;
        }
        if (source == "genRandom") {
            int count = 8;
            std::istringstream iss(input);
            iss >> count;
            generate(count);
            return true;
        }
        return false;
    }

public:
    SegmentTreeVisualizer() {
        layout = std::make_unique<TreeLayout>();
        weighted = false;
        hasNodeWeights = false;
        generatedDirected = false;
        values = {5, 3, 8, 1, 6, 2, 7, 4};
        setOpsFrom("query 1 6 update 3 9 query 2 8");
    }

    bool step() override {
        bool moved = advance();
        if (moved) stepCount++;
        syncVisuals();
        return moved;
    }

    // 1手少なく最初から流し直す (HeapVisualizer と同じ)
    void stepBack() override {
        if (stepCount <= 0) return;
        int target = stepCount - 1;
        resetRun();
        for (int i = 0; i < target; i++) step();
    }

    emscripten::val getState(emscripten::val params) override {
        emscripten::val state = GraphVisualizer::getState(params);

        emscripten::val vals = emscripten::val::array();
        for (int v : values) vals.call<void>("push", v);
        state.set("values", vals);

        emscripten::val list = emscripten::val::array();
        for (const Op& op : ops) list.call<void>("push", opLabel(op));
        state.set("ops", list);
        state.set("opIndex", opIndex);
        state.set("maxOps", MAX_OPS);
        state.set("maxValues", MAX_LEAVES);

        // 節点ごとの受け持ちと値。丸の中は値、まだ計算していなければ空
        emscripten::val ls = emscripten::val::array();
        emscripten::val segL = emscripten::val::array(), segR = emscripten::val::array();
        emscripten::val segLeft = emscripten::val::array(), segRight = emscripten::val::array();
        emscripten::val segValue = emscripten::val::array();
        for (const Node& node : nodes) {
            ls.call<void>("push", node.computed ? std::to_string(node.value) : std::string());
            segL.call<void>("push", node.l);
            segR.call<void>("push", node.r);
            segLeft.call<void>("push", node.left);
            segRight.call<void>("push", node.right);
            segValue.call<void>("push", node.computed ? emscripten::val(node.value)
                                                      : emscripten::val::null());
        }
        state.set("nodeLabels", ls);
        state.set("segL", segL);
        state.set("segR", segR);
        state.set("segLeft", segLeft);
        state.set("segRight", segRight);
        state.set("segValue", segValue);

        // 葉の下に添字。毎フレーム読まれるので、節点の今の座標に付いてくる
        emscripten::val labels = emscripten::val::array();
        for (int i = 0; i < (int)leafOf.size(); i++) {
            std::size_t o = (std::size_t)leafOf[i] * GraphData::NODE_STRIDE;
            emscripten::val l = emscripten::val::object();
            l.set("x", graph->nodeData[o]);
            l.set("y", graph->nodeData[o + 1] + 36.0f);
            l.set("text", std::to_string(i));
            l.set("align", "center");
            labels.call<void>("push", l);
        }
        state.set("labels", labels);

        state.set("phase", phase == Build ? "build" : phase == Update ? "update"
                           : phase == Query ? "query" : "done");
        state.set("opComplete", opComplete());
        state.set("lastNode", lastNode);
        state.set("lastKind", lastKind);
        state.set("queryL", ql);
        state.set("queryR", qr);
        emscripten::val takenNodes = emscripten::val::array();
        for (int u : taken) takenNodes.call<void>("push", u);
        state.set("takenNodes", takenNodes);
        // 最小 / 最大の単位元 (∞ / -∞) は数で渡せないので、何も取っていなければ null
        state.set("answer", taken.empty() && monoid != Sum ? emscripten::val::null()
                                                           : emscripten::val(answer));
        state.set("monoid", monoid == Sum ? "sum" : monoid == Min ? "min" : "max");
        state.set("finished", phase == Done);
        state.set("canStepBack", stepCount > 0);
        return state;
    }
};
