#pragma once
#include "GraphVisualizer.hpp"
#include "EasedLayout.hpp"
#include "GraphColors.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

// Fenwick 木 (BIT) の配置。添字の上に、箱 ft[i] を受け持ちの [i - lowbit(i) + 1, i] に
// またがらせて置く。段は lowbit の大きさで決まる。よく使われる図 (セグメント木から
// 右の子を消したもの) と同じ形で、操作で動かない。
//
// 節点は a[1..n] (番号 0..n-1) と ft[1..n] (番号 n..2n-1) の2n個。
//
// a は配列のページと同じ箱で描き、木の段から離して置く。円で描いて木のすぐ下に置くと、
// a と ft の奇数番 (受け持ちが1つ) の見分けが付かない。
class FenwickLayout : public EasedLayout {
public:
    static constexpr float ARRAY_HALF_WIDTH = 22.0f; // 配列のページ (ArrayVisualizer) と同じ
    static constexpr float PITCH = 2 * ARRAY_HALF_WIDTH + 8.0f; // a の箱の中心どうしの間隔
    static constexpr float ROW   = 56.0f; // 木の段と段の間隔
    static constexpr float ARRAY_GAP = 96.0f; // a と木のいちばん下の段の間隔

    static int lowbit(int i) { return i & -i; }

    static int level(int i) {
        int k = 0;
        while ((lowbit(i) >> k) > 1) k++;
        return k;
    }

    // ft[i] の箱の半幅。受け持ちが1つなら円、2つ以上なら a[i - lowbit + 1] から a[i] までの
    // 中心を、円と同じ余白で囲む
    static float halfWidthOf(int i) {
        return (float)(lowbit(i) - 1) * PITCH / 2.0f + GraphData::DEFAULT_HALF_WIDTH;
    }

protected:
    void computeTargets(GraphData* graph) override {
        (void)graph;
        targetX.assign(nodeSize, 0.0f);
        targetY.assign(nodeSize, 0.0f);
        int n = nodeSize / 2;
        for (int j = 1; j <= n; j++) targetX[j - 1] = (float)(j - 1) * PITCH;
        for (int i = 1; i <= n; i++) {
            targetX[n + i - 1] = (float)((i - lowbit(i)) + (i - 1)) * PITCH / 2.0f;
            targetY[n + i - 1] = -(ARRAY_GAP + (float)level(i) * ROW);
        }
    }
};

// Fenwick 木のビジュアライザ。添字は 1 始まり (lowbit で動くのが BIT そのもの)。
//
//   組み立て … ft[i] = a[i] から始めて、i = 1..n の順に ft[i] を ft[i + lowbit(i)] に足す。
//              足し先が n を超える i は飛ばす。1手で1回足す
//   add i x  … 1手で箱を1つ書き換える (ft[k] += x)。1手目で a[i] も書き換え、
//              k += lowbit(k) と右上へ登る
//   sum i    … 1手で箱を1つ足す。k -= lowbit(k) と左へ跳び、k = 0 で終わる。
//              答えは a[1] + ... + a[i]
//
// 集約は和だけ。最小 / 最大は引き算ができず、一般の BIT では成り立たない。
class FenwickVisualizer : public GraphVisualizer {
public:
    static constexpr int MAX_VALUES = 16;
    static constexpr int MAX_OPS = 10;
    static constexpr int MAX_VALUE = 99;

private:
    struct Op {
        bool isAdd = false;
        int i = 0, x = 0; // add i x / sum i
    };

    std::vector<int> values; // 入力した配列 (作り直すときの元)
    std::vector<Op> ops;

    std::vector<int> a;  // a[1..n]。0 番は使わない
    std::vector<int> ft; // ft[1..n]

    std::vector<int> buildOrder; // 組み立てで足し元になる i
    enum Phase { Build, Idle, Add, Sum, Done };
    Phase phase = Build;
    int buildAt = 0;
    int opIndex = 0;

    int cursor = 0;         // add / sum: 次に触る箱
    bool opStepped = false; // 今の操作で1手以上進んだ
    std::vector<int> taken; // sum: 足した箱
    int answer = 0;

    std::vector<char> touched; // 今の操作で前の手までに書いた / 足した箱
    int lastIndex = 0;         // 直前の手で書いた / 足した箱。無ければ 0
    int lastFrom = 0;          // 組み立て: 直前の手で読んだ箱
    int nextIndex = 0;         // 直前の手の次に触る箱
    std::string lastKind;      // "build" / "add" / "sum" / "empty"

    int stepCount = 0;

    int n() const { return (int)values.size(); }
    static int lowbit(int i) { return FenwickLayout::lowbit(i); }
    int boxOf(int i) const { return n() + i - 1; }

    static std::string lowered(const std::string& s) {
        std::string out = s;
        for (char& c : out) c = (char)std::tolower((unsigned char)c);
        return out;
    }

    void resetRun() {
        graph = std::make_unique<GraphData>(2 * MAX_VALUES, 0);
        graph->startNodeIndex = -1;
        a.assign(n() + 1, 0);
        ft.assign(n() + 1, 0);
        buildOrder.clear();
        for (int i = 1; i <= n(); i++) {
            a[i] = ft[i] = values[i - 1];
            if (i + lowbit(i) <= n()) buildOrder.push_back(i);
        }
        for (int j = 0; j < n(); j++) {
            graph->setNode(j, 0.0f, 0.0f, 0.0f, 0);
            graph->setHalfWidth(j, FenwickLayout::ARRAY_HALF_WIDTH);
        }
        for (int i = 1; i <= n(); i++) {
            graph->setNode(boxOf(i), 0.0f, 0.0f, 0.0f, 0);
            graph->setHalfWidth(boxOf(i), FenwickLayout::halfWidthOf(i));
        }

        buildAt = 0;
        opIndex = 0;
        cursor = 0;
        opStepped = false;
        taken.clear();
        answer = 0;
        touched.assign(n() + 1, 0);
        lastIndex = lastFrom = nextIndex = 0;
        lastKind.clear();
        stepCount = 0;
        phase = n() == 0 ? Done : Build;
        // 形は最初から決まっているので、寄せずにその場へ置く
        rebuildLayout();
        layout->finish(graph.get());
        syncVisuals();
    }

    void setValuesFrom(const std::string& text) {
        values.clear();
        std::istringstream iss(text);
        int v;
        while (iss >> v && n() < MAX_VALUES) values.push_back(std::clamp(v, -MAX_VALUE, MAX_VALUE));
        resetRun();
    }

    // "add 3 2 sum 7" のような並びを読む。番号は動かすときに端へ寄せる
    void setOpsFrom(const std::string& text) {
        ops.clear();
        std::istringstream iss(text);
        std::string word;
        while (iss >> word && (int)ops.size() < MAX_OPS) {
            std::string w = lowered(word);
            Op op;
            if (w == "add") op.isAdd = true;
            else if (w != "sum") continue; // 読めない語は飛ばす
            if (!(iss >> op.i)) break;
            if (op.isAdd) {
                if (!(iss >> op.x)) break;
                op.x = std::clamp(op.x, -MAX_VALUE, MAX_VALUE);
            }
            ops.push_back(op);
        }
        resetRun();
    }

    // 1〜9 の値と、sum と add を交互に並べた操作を作る
    void generate(int count) {
        count = std::clamp(count, 1, MAX_VALUES);
        values.clear();
        for (int i = 0; i < count; i++) values.push_back(randInt(9) + 1);

        std::string text;
        for (int k = 0; k < 5; k++) {
            if (k % 2 == 0) text += "sum " + std::to_string(randInt(count) + 1) + " ";
            else text += "add " + std::to_string(randInt(count) + 1) + " " +
                         std::to_string(randInt(9) + 1) + " ";
        }
        setOpsFrom(text);
    }

    // 操作を始める。前の操作の色はここで消す
    void beginOp(const Op& op) {
        std::fill(touched.begin(), touched.end(), 0);
        opStepped = false;
        if (op.isAdd) {
            phase = Add;
            cursor = std::clamp(op.i, 1, n());
        } else {
            phase = Sum;
            cursor = std::clamp(op.i, 0, n());
            taken.clear();
            answer = 0;
        }
    }

    bool opComplete() const {
        if (phase == Build) return buildAt >= (int)buildOrder.size();
        if (phase == Add) return cursor > n();
        if (phase == Sum) return opStepped && cursor <= 0;
        return true;
    }

    // 1手進める。true なら手が進んだ
    bool advance() {
        if (lastIndex > 0) touched[lastIndex] = 1;
        lastIndex = lastFrom = nextIndex = 0;
        lastKind.clear();
        if (phase == Done) return false;

        // 前の段階を終えていれば次の操作へ。終えた操作の色は、次の手まで残す
        if (opComplete()) {
            if (phase == Add || phase == Sum) opIndex++;
            phase = Idle;
            if (opIndex >= (int)ops.size()) { phase = Done; return false; }
            beginOp(ops[opIndex]);
        }
        bool firstOfOp = !opStepped;
        opStepped = true;

        switch (phase) {
        case Build: {
            int i = buildOrder[buildAt++];
            int j = i + lowbit(i);
            ft[j] += ft[i];
            lastIndex = j;
            lastFrom = i;
            nextIndex = buildAt < (int)buildOrder.size() ? buildOrder[buildAt] : 0;
            lastKind = "build";
            return true;
        }
        case Add: {
            int k = cursor;
            if (firstOfOp) a[k] += ops[opIndex].x;
            ft[k] += ops[opIndex].x;
            lastIndex = k;
            cursor = k + lowbit(k);
            nextIndex = cursor;
            lastKind = "add";
            return true;
        }
        case Sum: {
            if (cursor <= 0) { lastKind = "empty"; return true; }
            int k = cursor;
            answer += ft[k];
            taken.push_back(k);
            lastIndex = k;
            cursor = k - lowbit(k);
            nextIndex = cursor;
            lastKind = "sum";
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
        for (int i = 1; i <= n(); i++) {
            if (touched[i]) graph->setNodeColor(boxOf(i), NODE_START);
        }
        if (lastFrom > 0) graph->setNodeColor(boxOf(lastFrom), NODE_START);
        if (lastIndex > 0) graph->setNodeColor(boxOf(lastIndex), NODE_FRONTIER);
        // add で書き換えた a も、書き換えた手は橙、そのあとは青
        if (phase == Add) {
            int i = std::clamp(ops[opIndex].i, 1, n());
            graph->setNodeColor(i - 1, lastIndex == i ? NODE_FRONTIER : NODE_START);
        }
    }

    std::string opLabel(const Op& op) const {
        return op.isAdd ? "add " + std::to_string(op.i) + " " + std::to_string(op.x)
                        : "sum " + std::to_string(op.i);
    }

protected:
    const char* labelMode() const override { return "text"; }

    bool handleCommand(const std::string& source, const std::string& input) override {
        if (source == "setValues") { setValuesFrom(input); return true; }
        if (source == "setOps")    { setOpsFrom(input); return true; }
        if (source == "resetRun")  { resetRun(); return true; }
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
    FenwickVisualizer() {
        layout = std::make_unique<FenwickLayout>();
        weighted = false;
        hasNodeWeights = false;
        generatedDirected = false;
        values = {5, 3, 8, 1, 6, 2, 7, 4};
        setOpsFrom("sum 7 add 3 2 sum 6");
    }

    bool step() override {
        bool moved = advance();
        if (moved) stepCount++;
        syncVisuals();
        return moved;
    }

    // 1手少なく最初から流し直す (SegmentTreeVisualizer と同じ)
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
        state.set("maxValues", MAX_VALUES);

        // 節点の中の数。a[1..n] のあとに ft[1..n]
        emscripten::val ls = emscripten::val::array();
        emscripten::val fenA = emscripten::val::array(), fenTree = emscripten::val::array();
        for (int j = 1; j <= n(); j++) {
            ls.call<void>("push", std::to_string(a[j]));
            fenA.call<void>("push", a[j]);
        }
        for (int i = 1; i <= n(); i++) {
            ls.call<void>("push", std::to_string(ft[i]));
            fenTree.call<void>("push", ft[i]);
        }
        state.set("nodeLabels", ls);
        state.set("fenA", fenA);
        state.set("fenTree", fenTree);

        // a の下に添字。毎フレーム読まれるので、節点の今の座標に付いてくる
        emscripten::val labels = emscripten::val::array();
        for (int j = 1; j <= n(); j++) {
            std::size_t o = (std::size_t)(j - 1) * GraphData::NODE_STRIDE;
            emscripten::val l = emscripten::val::object();
            l.set("x", graph->nodeData[o]);
            l.set("y", graph->nodeData[o + 1] + 36.0f);
            l.set("text", std::to_string(j));
            l.set("align", "center");
            labels.call<void>("push", l);
        }
        state.set("labels", labels);

        state.set("phase", phase == Build ? "build" : phase == Add ? "add"
                           : phase == Sum ? "sum" : "done");
        state.set("opComplete", opComplete());
        state.set("lastIndex", lastIndex);
        state.set("lastFrom", lastFrom);
        state.set("nextIndex", nextIndex);
        state.set("lastKind", lastKind);
        emscripten::val takenBoxes = emscripten::val::array();
        for (int k : taken) takenBoxes.call<void>("push", k);
        state.set("taken", takenBoxes);
        state.set("answer", answer);
        state.set("finished", phase == Done);
        state.set("canStepBack", stepCount > 0);
        return state;
    }
};
