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
//   0 を移す   … 元の配列を左から見て、今のビットが 0 ならその場で別配列の左から詰める。
//                1 なら残す (見ただけ)
//   1 を移す   … 残った値を左から順に、別配列の 0 の後ろへ詰める
//   戻す       … 別配列をまとめて元の配列へ戻す (1手)
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
    enum Phase { MoveZeros, MoveOnes, CopyBack };

    Phase phase = MoveZeros;
    int pass = 0;      // 今のビット。0 が下から1ビット目
    int next = 0;      // 次に見る元の配列の位置
    int put = 0;       // 別配列の次に置く位置。0 を左から詰め、その後ろに 1 を詰める
    int zeros = 0;     // ここまでに見つけた 0 の数

    // 直前の手が何だったか
    int bit = -1;         // 見た / 置いた値のビット。無ければ -1
    bool looked = false;  // 見ただけで残した
    bool placed = false;  // 別配列へ置いた
    bool copied = false;  // まとめて戻した

    static int clampValue(int v) { return std::clamp(v, 0, MAX_RADIX_VALUE); }
    int bitOf(int v) const { return (v >> pass) & 1; }
    int workOf(int i) const { return rowSize() + i; } // 別配列の同じ位置

    // 今のビットが 0 ならその場で別配列へ、1 なら残す
    void moveIfZero() {
        bit = bitOf(valueAt(next));
        if (bit == 0) {
            carryValue(next, workOf(put++)); // 元のマスは空の箱で残る
            focusA = workOf(put - 1);
            placed = true;
            zeros++;
        } else {
            focusA = next; // 見ただけ。動かさないので赤
            looked = true;
        }
        next++;
    }

    // 残った値 (ビットが 1) を、別配列の 0 の後ろへ詰める
    void moveOne() {
        bit = bitOf(valueAt(next));
        carryValue(next, workOf(put++));
        focusA = workOf(put - 1);
        placed = true;
        next++;
    }

    // 別配列をまとめて元の配列へ戻す。1手
    void copyAll() {
        for (int i = 0; i < rowSize(); i++) carryValue(workOf(i), i);
        copied = true;
        // 最後のビットならこれで並び終わり
        if (pass == DIGITS - 1) settleAll();
    }

    // 残っている (まだ移していない) 次の位置。無ければ rowSize()
    int nextRemaining(int from) const {
        while (from < rowSize() && emptySlot[from]) from++;
        return from;
    }

protected:
    // 下の段が別配列。長さは元と同じ
    int extraSlots() const override { return rowSize(); }

    void resetAlgorithm() override {
        phase = MoveZeros;
        pass = 0;
        next = put = zeros = 0;
        bit = -1;
        looked = placed = copied = false;
        focusA = focusB = -1;
        if (rowSize() <= 0) finished = true;
    }

    void syncVisuals() override {
        if (!graph) return;
        graph->resetColors();
        paintSettled();
        if (finished) return;
        // 見ただけなら赤、置いたら緑。まとめて戻したときは全部が緑
        if (copied) {
            for (int i = 0; i < rowSize(); i++) graph->setNodeColor(i, NODE_PATH);
        } else {
            graph->setNodeColor(focusA, looked ? NODE_VISITING : NODE_PATH);
        }
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
        looked = placed = copied = false;
        focusA = focusB = -1;

        // 局面の切り替えは手を消費しない
        for (;;) {
            switch (phase) {
            case MoveZeros:
                if (next < rowSize()) { moveIfZero(); return true; }
                phase = MoveOnes;
                next = nextRemaining(0);
                break;

            case MoveOnes:
                if (next < rowSize()) {
                    moveOne();
                    next = nextRemaining(next);
                    return true;
                }
                phase = CopyBack;
                break;

            case CopyBack:
                copyAll();
                if (pass + 1 >= DIGITS) {
                    finished = true; // pass は最後のビットのまま
                } else {
                    pass++;
                    phase = MoveZeros;
                    next = put = zeros = 0;
                }
                return true;
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
        state.set("phase", phase == MoveZeros ? "zeros" : phase == MoveOnes ? "ones" : "copy");
        state.set("bit", bit);
        state.set("zeros", zeros);
        state.set("looked", looked);
        state.set("placed", placed);
        state.set("copied", copied);

        // 値は常に 4 ビットの2進で書き、今のビットに下線を引く
        state.set("digitBase", 2);
        state.set("digitCount", DIGITS);
        state.set("digitFocus", finished ? -1 : pass);
        return state;
    }
};
