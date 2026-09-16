#pragma once
#include "ArrayVisualizer.hpp"
#include "GraphColors.hpp"
#include <algorithm>
#include <string>
#include <vector>

// バケットソートのビジュアライザ。
//
// **上の段が配列、その下がバケット。** 値の範囲でバケットが決まる。
// 3つの局面を順にたどる。
//
//   配る               … 配列の左から1つ取り、範囲のバケットの末尾へ移す
//   バケットの中を並べる … バケットを順に見て、挿入ソートと同じ3手で並べる
//                        (取り出す / ずらす / 差し込む)。空のバケットも1手かけて見る
//   集める             … バケットを順に、先頭から配列へ戻す。戻した範囲が確定
//
// バケットの段は**入っている数だけ**マスが並ぶ。使っていないマスは描かない。
// 段は左から詰めるので、畳んで場所を詰める必要は無い (デックと違う点)。
// 配列の段は空いたマスを空の箱として残す。値がそこへ戻ってくるので、形は保っておく。
//
// 集めるときも、取ったマスは消すだけ。残りを左へ詰めると動いて見え、
// 取った動きと紛れる。
class BucketSortVisualizer : public ArrayVisualizer {
public:
    // 値の上限が 99 なので、幅 20 のバケット5つで全部入る
    static constexpr int BUCKETS = 5;
    static constexpr int BUCKET_WIDTH = (MAX_VALUE + 1) / BUCKETS;

private:
    enum Phase { Scatter, SortBuckets, Gather };

    Phase phase = Scatter;
    int next = 0;          // 配る: 次に取る配列の位置 / 集める: 次に戻す位置
    int bucket = 0;        // 並べる / 集める: 今のバケット
    int take = 0;          // 集める: 今のバケットの次に取る位置
    std::vector<int> fill; // バケットごとの入っている数

    // 挿入ソートの3手。hole は節点の番号、sortNext はバケットの中の位置
    int sortNext = 0;
    int hole = -1;
    int held = -1;
    int droppedAt = -1;

    // 直前の手が何だったか
    int acted = -1;            // 手を打ったバケット。無ければ -1
    bool scattered = false;
    bool gathered = false;
    bool visitedEmpty = false;

    int slotOf(int b, int i) const { return rowSize() * (b + 1) + i; }
    int bucketOf(int v) const { return std::clamp(v / BUCKET_WIDTH, 0, BUCKETS - 1); }

    // 集めるときに、そのバケットから取り終えた数
    int takenOf(int b) const {
        if (phase != Gather) return 0;
        return b < bucket ? fill[b] : b == bucket ? take : 0;
    }

    // そのマスを描くか。配列は常に、バケットは入っていてまだ取っていないものだけ
    bool isDrawn(int node) const {
        if (node < rowSize()) return true;
        int b = node / rowSize() - 1, i = node % rowSize();
        return i < fill[b] && i >= takenOf(b);
    }

    void scatterOne() {
        int v = valueAt(next);
        int b = bucketOf(v);
        int dest = slotOf(b, fill[b]);
        moveValue(next, dest); // 配列のマスが空く
        fill[b]++;
        focusA = dest;
        acted = b;
        scattered = true;
        next++;
    }

    // 挿入ソートと同じ3手を、今のバケットの中だけで打つ
    void insertionMove() {
        int base = slotOf(bucket, 0);
        acted = bucket;

        // 取り出す
        if (hole < 0) {
            hole = base + sortNext;
            held = valueAt(hole);
            return;
        }

        // 左隣の方が大きいので、それを右へずらして空きを左へ移す
        if (hole > base && valueAt(hole - 1) > held) {
            focusA = hole;
            swapValues(hole, hole - 1);
            hole--;
            return;
        }

        // 左隣以上なので、ここが入る場所
        droppedAt = hole;
        hole = -1;
        held = -1;
        sortNext++;
    }

    void gatherOne() {
        int src = slotOf(bucket, take);
        moveValue(src, next);
        focusA = next;
        acted = bucket;
        gathered = true;
        take++;
        next++;
        markSettled(0, next - 1);
        if (next >= rowSize()) finished = true;
    }

protected:
    // バケットの段は配列と同じ長さ (全部が1つのバケットに入ることがある)
    int extraSlots() const override { return rowSize() * BUCKETS; }

    void resetAlgorithm() override {
        phase = Scatter;
        next = bucket = take = 0;
        fill.assign(BUCKETS, 0);
        sortNext = 0;
        hole = held = droppedAt = -1;
        acted = -1;
        scattered = gathered = visitedEmpty = false;
        focusA = focusB = -1;
        if (rowSize() <= 0) finished = true;
    }

    void syncVisuals() override {
        if (!graph) return;
        graph->resetColors();
        paintSettled();
        // 今並べているバケット
        if (!finished && phase == SortBuckets && bucket < BUCKETS) {
            for (int i = 0; i < fill[bucket]; i++) {
                graph->setNodeColor(slotOf(bucket, i), NODE_RANGE);
            }
        }
        paintFocus();
        if (finished) return;
        if (hole >= 0) graph->setNodeColor(hole, NODE_VISITING);
        if (droppedAt >= 0) graph->setNodeColor(droppedAt, NODE_PATH);
    }

    bool advance() override {
        if (finished) return false;
        acted = -1;
        scattered = gathered = visitedEmpty = false;
        droppedAt = -1;
        focusA = focusB = -1;

        // 局面の切り替えは手を消費しない。見えることが起きる手まで進める
        for (;;) {
            switch (phase) {
            case Scatter:
                if (next < rowSize()) { scatterOne(); return true; }
                phase = SortBuckets;
                bucket = 0;
                sortNext = 0;
                break;

            case SortBuckets:
                if (bucket >= BUCKETS) {
                    phase = Gather;
                    next = bucket = take = 0;
                    break;
                }
                // 空のバケットも見る。ループは全部のバケットを訪れる
                if (fill[bucket] == 0) {
                    acted = bucket;
                    visitedEmpty = true;
                    bucket++;
                    return true;
                }
                if (hole < 0 && sortNext >= fill[bucket]) {
                    bucket++;
                    sortNext = 0;
                    break;
                }
                insertionMove();
                return true;

            case Gather:
                if (bucket >= BUCKETS) {
                    // 全部戻し終えている。gatherOne が finished を立てるので、
                    // ここへ来るのは配列が空のときだけ
                    finished = true;
                    syncVisuals();
                    return false;
                }
                if (take >= fill[bucket]) {
                    bucket++;
                    take = 0;
                    break;
                }
                gatherOne();
                return true;
            }
        }
    }

public:
    BucketSortVisualizer() {
        // 範囲に散らばる並び。全部が 0–19 だとバケットの意味が見えない
        setValuesFrom("42 7 88 23 65 51 19 94 36 70");
    }

    emscripten::val getState(emscripten::val params) override {
        emscripten::val state = ArrayVisualizer::getState(params);

        state.set("phase", phase == Scatter ? "scatter" : phase == SortBuckets ? "sort" : "gather");
        state.set("bucket", acted);
        state.set("scattered", scattered);
        state.set("gathered", gathered);
        state.set("visitedEmpty", visitedEmpty);

        emscripten::val fills = emscripten::val::array();
        for (int b = 0; b < BUCKETS; b++) fills.call<void>("push", fill[b]);
        state.set("bucketFill", fills);

        // 持ち上げている値と空いたマス。描画側が段の上に浮かせる
        state.set("heldValue", held);
        state.set("holeIndex", hole);
        state.set("droppedAt", droppedAt);

        emscripten::val hidden = emscripten::val::array();
        for (int i = 0; graph && i < graph->nodeCount(); i++) {
            if (!isDrawn(i)) hidden.call<void>("push", i);
        }
        state.set("hiddenSlots", hidden);

        // 段の左に書く範囲。この段が何を指すかは画面に要る
        emscripten::val labels = emscripten::val::array();
        for (int b = 0; b < BUCKETS; b++) {
            emscripten::val l = emscripten::val::object();
            l.set("x", -14.0f);
            l.set("y", (float)(b + 1) * LineLayout::ROW_GAP);
            l.set("text", std::to_string(b * BUCKET_WIDTH) + "–" +
                          std::to_string((b + 1) * BUCKET_WIDTH - 1));
            labels.call<void>("push", l);
        }
        state.set("labels", labels);
        return state;
    }
};
