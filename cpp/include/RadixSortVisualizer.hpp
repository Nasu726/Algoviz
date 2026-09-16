#pragma once
#include "ArrayVisualizer.hpp"
#include "GraphColors.hpp"
#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

// 基数ソート (LSD) のビジュアライザ。
//
// 値は 0〜99 の2桁。**桁ごとに 1の位 → 10の位 の順で、2回まわす。** 1回は2つの局面。
//   配る   … 配列の左から1つ取り、今の桁の値のバケットの下端へ移す
//   集める … バケットを 0 から順に、上から1つずつ配列へ戻す。空のバケットも1手見る
//
// 比べる手は無い。**安定**なのが見どころで、バケットは上から順に取るので、
// 同じ桁の値どうしは入れた順のまま並ぶ。10の位でまわしても1の位の順が崩れない。
// 全部1桁の入力でも2回まわす (ループは全部の桁を訪れる)。
//
// **上の段が配列、その下に 0〜9 のバケットが列として並ぶ。** 節点は
//   1段目:   [配列 0..n-1] [余り]
//   k+1段目: 列 d の深さ k のマス = W*(k+1) + pad + d
// 1段の幅 W は配列の数と 10 の大きい方。余りを両側に分けて列を配列の下の
// 真ん中に置く。深さは最大 n。使っていないマスは描かない。
//
// 配列のマスは空の箱で残し、値だけ動かす (carryValue)。
class RadixSortVisualizer : public ArrayVisualizer {
public:
    static constexpr int DIGITS = 4;   // ビットの数
    static constexpr int BASE = 2;
    static constexpr int MAX_RADIX_VALUE = (1 << DIGITS) - 1; // 15

    // 画面に収める深さの下限。バケットが深くなるたびに拡大率が変わると
    // マスの大きさが安定しないので、はじめからこの段数ぶん取っておく
    static constexpr int VIEW_MIN_DEPTH = 4;

private:
    enum Phase { Scatter, Gather };

    Phase phase = Scatter;
    int pass = 0;          // 今の桁。0 が1の位
    int next = 0;          // 配る: 次に取る位置 / 集める: 次に戻す位置
    int bucket = 0;        // 集める: 今見ているバケット
    int take = 0;          // 集める: そのバケットの次に取る位置
    bool looked = false;   // 集める: 今のバケットを1手以上見たか
    std::vector<int> fill; // バケットごとの数

    // 直前の手が何だったか
    int acted = -1;        // 手を打ったバケット。無ければ -1
    bool scattered = false;
    bool gathered = false;
    bool sawEmpty = false;

    static int clampValue(int v) { return std::clamp(v, 0, MAX_RADIX_VALUE); }

    int digitOf(int v) const {
        int d = v;
        for (int i = 0; i < pass; i++) d /= BASE;
        return d % BASE;
    }

    int width() const { return std::max(rowSize(), BASE); }
    int pad() const { return (width() - BASE) / 2; }
    int slotOf(int d, int k) const { return width() * (k + 1) + pad() + d; }

    // 今いちばん深いバケットの段。空なら 0 (配列の段だけ)
    int deepest() const {
        int d = 0;
        for (int b = 0; b < BASE; b++) d = std::max(d, fill[b]);
        return d;
    }

    // 集めるときに、そのバケットから取り終えた数
    int takenOf(int b) const {
        if (phase != Gather) return 0;
        return b < bucket ? fill[b] : b == bucket ? take : 0;
    }

    bool isDrawn(int node) const {
        if (node < rowSize()) return true;
        int row = node / width(), col = node % width() - pad();
        if (row < 1 || col < 0 || col >= BASE) return false;
        int k = row - 1;
        return k < fill[col] && k >= takenOf(col);
    }

    void scatterOne() {
        int v = valueAt(next);
        int d = digitOf(v);
        int dest = slotOf(d, fill[d]);
        carryValue(next, dest); // 配列のマスは空の箱で残る
        fill[d]++;
        focusA = dest;
        acted = d;
        scattered = true;
        next++;
    }

    void gatherOne() {
        int src = slotOf(bucket, take);
        carryValue(src, next);
        focusA = next;
        acted = bucket;
        gathered = true;
        looked = true;
        take++;
        next++;
        // 最後の桁を集めているときだけ、戻した範囲が確定
        if (pass == DIGITS - 1) markSettled(0, next - 1);
    }

    void enterBucket(int b) {
        bucket = b;
        take = 0;
        looked = false;
    }

    bool lastBucketOfLastPass() const { return bucket == BASE - 1 && pass == DIGITS - 1; }

protected:
    // 深さは最大 n。1段目の余りも含めて、2段目から n 段
    int extraSlots() const override { return width() * (rowSize() + 1) - rowSize(); }

    void configureCells() override { line->setPerRow(width()); }

    void resetAlgorithm() override {
        phase = Scatter;
        pass = 0;
        next = 0;
        fill.assign(BASE, 0);
        enterBucket(0);
        acted = -1;
        scattered = gathered = sawEmpty = false;
        focusA = focusB = -1;
        // 配列より後ろのマスは空ではなく、描かないだけ
        for (int i = rowSize(); i < (int)emptySlot.size(); i++) emptySlot[i] = 0;
        if (rowSize() <= 0) finished = true;
    }

    void syncVisuals() override {
        if (!graph) return;
        graph->resetColors();
        paintSettled();
        paintFocus();
    }

    bool handleCommand(const std::string& source, const std::string& input) override {
        // 値を 0〜15 に寄せてから受け取る
        if (source == "setValues") {
            values.clear();
            std::istringstream iss(input);
            int v;
            while (iss >> v && (int)values.size() < MAX_VALUES) values.push_back(clampValue(v));
            resetRun();
            return true;
        }
        // 範囲が狭いので重複を許す。同じ値が並んでも順が保たれるのが見える
        if (source == "genRandom") {
            int count = 12;
            std::istringstream iss(input);
            iss >> count;
            count = std::clamp(count, 1, MAX_VALUES);
            values.clear();
            for (int i = 0; i < count; i++) values.push_back(randInt(MAX_RADIX_VALUE + 1));
            resetRun();
            return true;
        }
        return ArrayVisualizer::handleCommand(source, input);
    }

    bool advance() override {
        if (finished) return false;
        acted = -1;
        scattered = gathered = sawEmpty = false;
        focusA = focusB = -1;

        // 局面の切り替えは手を消費しない。見えることが起きる手まで進める
        for (;;) {
            switch (phase) {
            case Scatter:
                if (next < rowSize()) { scatterOne(); return true; }
                phase = Gather;
                next = 0;
                enterBucket(0);
                break;

            case Gather:
                if (bucket >= BASE) {
                    // この桁を集め終えた。次の桁へ。全部の桁を見たら終わり
                    // (最後の桁の最後のバケットで finished を立てるので、
                    //  ここへ来るのは配列が空のときだけ)
                    pass++;
                    if (pass >= DIGITS) {
                        finished = true;
                        syncVisuals();
                        return false;
                    }
                    phase = Scatter;
                    next = 0;
                    fill.assign(BASE, 0);
                    break;
                }
                if (take < fill[bucket]) {
                    gatherOne();
                    if (take >= fill[bucket] && lastBucketOfLastPass()) finished = true;
                    return true;
                }
                if (!looked) {
                    // 空のバケットを見る。何も動かないが、1手かける
                    looked = true;
                    sawEmpty = true;
                    acted = bucket;
                    if (lastBucketOfLastPass()) finished = true;
                    return true;
                }
                enterBucket(bucket + 1);
                break;
            }
        }
    }

public:
    RadixSortVisualizer() {
        // 下のビットが同じ値が複数ある並び。安定に並ぶことが見える
        setValuesFrom("5 12 3 10 7 1 14 6 9 2");
    }

    emscripten::val getState(emscripten::val params) override {
        emscripten::val state = ArrayVisualizer::getState(params);

        state.set("pass", pass);
        state.set("digits", DIGITS);
        state.set("phase", phase == Scatter ? "scatter" : "gather");
        state.set("bucket", acted);
        state.set("scattered", scattered);
        state.set("gathered", gathered);
        state.set("sawEmpty", sawEmpty);

        // 値は常に 4 ビットの2進で書き、今のビットに下線を引く
        state.set("digitBase", BASE);
        state.set("digitCount", DIGITS);
        state.set("digitFocus", finished ? -1 : pass);

        emscripten::val hidden = emscripten::val::array();
        for (int i = 0; graph && i < graph->nodeCount(); i++) {
            if (!isDrawn(i)) hidden.call<void>("push", i);
        }
        state.set("hiddenSlots", hidden);

        // 列の見出し 0 と 1。集めるときに見ている列は赤
        int looking = (phase == Gather && !finished && bucket < BASE) ? bucket : -1;
        emscripten::val labels = emscripten::val::array();
        for (int d = 0; d < BASE; d++) {
            emscripten::val l = emscripten::val::object();
            l.set("x", line->targetXOf(slotOf(d, 0)));
            l.set("y", LineLayout::ROW_GAP - CELL_HALF_WIDTH - 12.0f);
            l.set("text", std::to_string(d));
            l.set("align", "center");
            if (d == looking) l.set("color", 0xe74c3c);
            labels.call<void>("push", l);
        }
        state.set("labels", labels);

        // 深さ n の段まで節点があるので、今使っている段までを画面に収める
        emscripten::val bounds = emscripten::val::array();
        bounds.call<void>("push", line->targetXOf(0));
        bounds.call<void>("push", 0.0f);
        bounds.call<void>("push", line->targetXOf(width() - 1));
        bounds.call<void>("push", (float)std::max(deepest(), VIEW_MIN_DEPTH) * LineLayout::ROW_GAP);
        state.set("viewBounds", bounds);
        return state;
    }
};
