#pragma once
#include "ArrayVisualizer.hpp"
#include "GraphColors.hpp"
#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

// 基数ソート (LSD、2進) のビジュアライザ。
//
// 値は 0〜15 の 4 ビットで、**マスには2進で書く。** ビットごとに下から順に
// 4 回まわす。1回は3つの局面で、**別配列に配置してから元に戻す**。
//   数える   … 元の配列を左から見て、今のビットが 0 の値を数える
//   配置する … 元の配列を左から見て、ビットが 0 なら別配列の前から、1 なら
//              「0 の個数」の位置から後ろへ、それぞれ詰めて置く
//   戻す     … 別配列を左から元の配列へ戻す
//
// 比べる手は無い。**安定**なのが見どころで、左から走査して前から詰めるので、
// 同じビットの値どうしは入れた順のまま並ぶ。上のビットでまわしても、下のビットで
// 決めた順が崩れない。全部の値が同じビットでも 4 回まわす (ループは全部のビットを
// 訪れる)。
//
// 1つの配列で済ませる方法 (radix exchange sort: 上のビットから両端で入れ替えて分ける)
// は安定でない。安定に並べるには別の置き場が要るので、その形をそのまま見せる。
//
// 10進でなく2進なのは、桁の数が少ないほど「桁ごとに分ける」が読めるため。
// 4 ビットなら文字がマスに収まる。
//
// **上の段が元の配列、下の段が別配列。** 値は carryValue で動かし、
// 空いたマスは空の箱で残す (次の局面で戻ってくる)。
class RadixSortVisualizer : public ArrayVisualizer {
public:
    static constexpr int DIGITS = 4;   // ビットの数
    static constexpr int MAX_RADIX_VALUE = (1 << DIGITS) - 1; // 15

private:
    enum Phase { Count, Place, CopyBack };

    Phase phase = Count;
    int pass = 0;      // 今のビット。0 が下から1ビット目
    int next = 0;      // 各局面で、次に見る元の配列 (戻すときは別配列) の位置
    int zeros = 0;     // 数える: ここまでの 0 の数。配置する: 0 の個数 (= 1 を置き始める位置)
    int putZero = 0;   // 配置する: 次に 0 を置く位置
    int putOne = 0;    // 配置する: 次に 1 を置く位置

    // 直前の手が何だったか
    int bit = -1;         // 見た / 置いた値のビット。無ければ -1
    bool counted = false;
    bool placed = false;
    bool copied = false;

    static int clampValue(int v) { return std::clamp(v, 0, MAX_RADIX_VALUE); }
    int bitOf(int v) const { return (v >> pass) & 1; }
    int workOf(int i) const { return rowSize() + i; } // 別配列の同じ位置

    void countOne() {
        bit = bitOf(valueAt(next));
        if (bit == 0) zeros++;
        focusA = next; // 見ている値。動かさないので赤
        counted = true;
        next++;
    }

    void placeOne() {
        bit = bitOf(valueAt(next));
        int dest = bit == 0 ? putZero++ : putOne++;
        carryValue(next, workOf(dest)); // 元のマスは空の箱で残る
        focusA = workOf(dest);
        placed = true;
        next++;
    }

    void copyOne() {
        carryValue(workOf(next), next);
        focusA = next;
        copied = true;
        // 最後のビットを戻しているときだけ、戻した範囲が確定
        if (pass == DIGITS - 1) markSettled(0, next);
        next++;
    }

protected:
    // 下の段が別配列。長さは元と同じ
    int extraSlots() const override { return rowSize(); }

    void resetAlgorithm() override {
        phase = Count;
        pass = 0;
        next = zeros = putZero = putOne = 0;
        bit = -1;
        counted = placed = copied = false;
        focusA = focusB = -1;
        if (rowSize() <= 0) finished = true;
    }

    void syncVisuals() override {
        if (!graph) return;
        graph->resetColors();
        paintSettled();
        if (finished) return;
        // 数えるときは見ただけ (赤)、置く / 戻すときは動かした (緑)
        graph->setNodeColor(focusA, counted ? NODE_VISITING : NODE_PATH);
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
        bit = -1;
        counted = placed = copied = false;
        focusA = focusB = -1;

        // 局面の切り替えは手を消費しない
        for (;;) {
            if (next < rowSize()) {
                switch (phase) {
                case Count:    countOne(); return true;
                case Place:    placeOne(); return true;
                case CopyBack: copyOne();
                    if (next >= rowSize() && pass == DIGITS - 1) finished = true;
                    return true;
                }
            }
            // この局面を終えた
            next = 0;
            switch (phase) {
            case Count:
                phase = Place;
                putZero = 0;
                putOne = zeros; // 1 は 0 の後ろから
                break;
            case Place:
                phase = CopyBack;
                break;
            case CopyBack:
                pass++;
                if (pass >= DIGITS) {
                    // 最後の戻すで finished を立てるので、ここへ来るのは配列が空のときだけ
                    finished = true;
                    syncVisuals();
                    return false;
                }
                phase = Count;
                zeros = 0;
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
        state.set("phase", phase == Count ? "count" : phase == Place ? "place" : "copy");
        state.set("bit", bit);
        state.set("zeros", zeros);
        state.set("looked", counted);
        state.set("placed", placed);
        state.set("copied", copied);

        // 値は常に 4 ビットの2進で書き、今のビットに下線を引く
        state.set("digitBase", 2);
        state.set("digitCount", DIGITS);
        state.set("digitFocus", finished ? -1 : pass);
        return state;
    }
};
