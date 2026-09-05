#pragma once
#include "GraphVisualizer.hpp"
#include "LineLayout.hpp"
#include "GraphColors.hpp"
#include <algorithm>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

// 配列を一列に並べて見せるものの基底。ソートや探索がこれを継承する。
//
// **節点の番号がそのまま添字で、動くのは値だけ。** 配列の中身は節点の重み欄に
// 置き、描画側は labelMode "value" でそれを箱の中に出す。
//
// 箱の幅は値によらず一定にしてある。桁数で変えるとソートの途中で箱が伸び縮みし、
// 値が動いたのか幅が変わったのか分からなくなる。
//
// 派生クラスが書くのは advance() (1手進める) だけ。値の並べ直し・作り直し・
// 「1手少なく流し直す」stepBack は共通なのでここに置く。
//
// マージソートのように作業用の場所が要るものは extraSlots で節点を足す。
// 足したぶんは下の段に並び、値の入っていないマスは emptySlots で空に見せる。
class ArrayVisualizer : public GraphVisualizer {
public:
    // 上限が 20 なのは、隣どうしを n 回ずつ比べるソートで n が大きいと
    // 1手ずつ見るものではなくなるため。MAX_NODES (50) はここでは効かない。
    static constexpr int MAX_VALUES = 20;
    static constexpr int MAX_VALUE  = 99;
    static constexpr float CELL_HALF_WIDTH = 22.0f;

protected:
    std::vector<int> values; // 入力の並び。作り直すときの元になる

    int stepCount = 0;
    bool replaying = false;
    bool finished = false;

    // 位置ごとに「もう動かない (または並んでいる)」。ソートによって
    // 左から埋まるもの、右から埋まるもの、両端から埋まるものがある。
    std::vector<char> settled;

    // 今その手で見ている位置。無ければ -1
    int focusA = -1, focusB = -1;
    // 直前の手で入れ替えたか。swapValues が立て、次の手の頭で下ろす
    bool justSwapped = false;

    // まだ値の入っていないマス。描画側が空の箱として描く
    std::vector<char> emptySlot;

    LineLayout* line = nullptr; // layout の実体。段の設定に使う

    // 1段に並べる数。配列そのものの長さ
    int rowSize() const { return (int)values.size(); }
    // 元の配列の後ろに足す作業用のマスの数
    virtual int extraSlots() const { return 0; }

    int valueAt(int i) const {
        return (int)graph->nodeData[(std::size_t)i * GraphData::NODE_STRIDE + 2];
    }

    void setValueAt(int i, int v) {
        graph->nodeData[(std::size_t)i * GraphData::NODE_STRIDE + 2] = (float)v;
    }

    // 値を入れ替える。今いる座標も入れ替えるので、EasedLayout が元の位置へ
    // 戻す間に「値が移動した」ように見える。
    void swapValues(int a, int b) {
        int va = valueAt(a), vb = valueAt(b);
        setValueAt(a, vb);
        setValueAt(b, va);
        swapNodePositions(a, b);
        justSwapped = true;
    }

    // 値を別のマスへ移す。移した先が埋まり、元のマスが空になる。
    // 入れ替えとして書くので、配列の中身は全体では入力の並べ替えのまま。
    void moveValue(int from, int to) {
        swapValues(from, to);
        if (from >= 0 && from < (int)emptySlot.size()) emptySlot[from] = 1;
        if (to >= 0 && to < (int)emptySlot.size()) emptySlot[to] = 0;
    }

    void markSettled(int from, int to) {
        for (int i = std::max(0, from); i <= to && i < (int)settled.size(); i++) settled[i] = 1;
    }

    void settleAll() { markSettled(0, (int)settled.size() - 1); }

    int settledCount() const {
        return (int)std::count(settled.begin(), settled.end(), (char)1);
    }

    // 派生クラスが実装する。1手進めたら true、終わっていたら false。
    virtual bool advance() = 0;

    // 派生クラスの実行状態を初期化する
    virtual void resetAlgorithm() {}

    void paintSettled() {
        for (int i = 0; i < (int)settled.size(); i++) {
            if (settled[i]) graph->setNodeColor(i, NODE_VISITED);
        }
    }

    // 並び終えたら全部が確定した色になる。比べている2つが残ると
    // 「まだ何かしている」ように見える
    void paintFocus() {
        if (finished) return;
        int color = justSwapped ? NODE_PATH : NODE_VISITING;
        graph->setNodeColor(focusA, color);
        graph->setNodeColor(focusB, color);
    }

    // 色を状態から塗り直す。差分で塗ると戻したときに前の色が残る。
    // 確定した位置と、今見ている2つ。ほかに色が要るものは、下地を塗ってから
    // paintSettled / paintFocus を呼ぶ形で組み立て直す。
    virtual void syncVisuals() {
        if (!graph) return;
        graph->resetColors();
        paintSettled();
        paintFocus();
    }

    void buildArray() {
        int total = rowSize() + extraSlots();
        graph = std::make_unique<GraphData>(MAX_VALUES * 2, 0);
        graph->startNodeIndex = -1;
        for (int i = 0; i < total; i++) {
            float v = i < rowSize() ? (float)values[i] : 0.0f;
            graph->setNode(i, 0.0f, 0.0f, v, 0);
            graph->setHalfWidth(i, CELL_HALF_WIDTH);
        }
        // 足したぶんは下の段。最初は空
        emptySlot.assign(total, 0);
        for (int i = rowSize(); i < total; i++) emptySlot[i] = 1;

        if (line) line->setPerRow(rowSize());
        rebuildLayout();
        // 作り直した直後は動かさない。左から流れ込んでくる必要は無い
        layout->finish(graph.get());
    }

    void resetRun() {
        buildArray();
        stepCount = 0;
        finished = false;
        settled.assign(values.size(), 0);
        focusA = focusB = -1;
        justSwapped = false;
        resetAlgorithm();
        syncVisuals();
    }

    void setValuesFrom(const std::string& text) {
        values.clear();
        std::istringstream iss(text);
        int v;
        while (iss >> v && (int)values.size() < MAX_VALUES) values.push_back(v);
        resetRun();
    }

    void generateValues(int count) {
        count = std::clamp(count, 1, MAX_VALUES);
        std::vector<int> pool(MAX_VALUE);
        std::iota(pool.begin(), pool.end(), 1);
        std::shuffle(pool.begin(), pool.end(), rng);
        values.assign(pool.begin(), pool.begin() + count);
        resetRun();
    }

    const char* labelMode() const override { return "value"; }

    bool handleCommand(const std::string& source, const std::string& input) override {
        if (source == "setValues") { setValuesFrom(input); return true; }
        if (source == "resetRun")  { resetRun(); return true; }
        if (source == "genRandom") {
            int count = 12;
            std::istringstream iss(input);
            iss >> count;
            generateValues(count);
            return true;
        }
        return false;
    }

public:
    ArrayVisualizer() {
        // 基底のコンストラクタが一般グラフを作っているので、配列用に置き換える
        auto lineLayout = std::make_unique<LineLayout>();
        line = lineLayout.get();
        layout = std::move(lineLayout);
        weighted = false;
        hasNodeWeights = false;
        generatedDirected = false;
        skipExtension = false; // 入れ替わる様子を見せたいので収束を飛ばさない
    }

    bool step() override {
        justSwapped = false;
        if (!advance()) return false;
        stepCount++;
        syncVisuals();
        return true;
    }

    // 1手少なく最初から流し直す。入れ替えを巻き戻すより作り直す方が確実。
    void stepBack() override {
        if (stepCount <= 0) return;
        int target = stepCount - 1;

        replaying = true;
        resetRun();
        for (int i = 0; i < target; i++) step();
        replaying = false;

        syncVisuals();
    }

    emscripten::val getState(emscripten::val params) override {
        emscripten::val state = GraphVisualizer::getState(params);

        // 今の並び。入力の並びではなく、途中まで動いた結果。
        // 作業用のマスは配列そのものではないので出さない
        emscripten::val vs = emscripten::val::array();
        for (int i = 0; i < rowSize() && i < graph->nodeCount(); i++) {
            vs.call<void>("push", valueAt(i));
        }
        state.set("values", vs);

        emscripten::val empties = emscripten::val::array();
        for (int i = 0; i < (int)emptySlot.size(); i++) {
            if (emptySlot[i]) empties.call<void>("push", i);
        }
        state.set("emptySlots", empties);
        state.set("rowSize", rowSize());

        state.set("finished", finished);
        state.set("canStepBack", stepCount > 0);
        state.set("maxValues", MAX_VALUES);
        state.set("focusA", focusA);
        state.set("focusB", focusB);
        state.set("swapped", justSwapped);
        state.set("settledCount", settledCount());
        return state;
    }
};
