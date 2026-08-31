// AlgoViz コアエンジンのテスト
//
// ネイティブの C++ コンパイラは使わず、本番と同じ emcc で WASM にビルドして
// Node で実行する。本番と同一のコンパイラ・標準ライブラリで検証できる。
//
//   npm run test:core
//
#include <emscripten/val.h>
#include <iostream>
#include <string>
#include <vector>

#include "../cpp/include/Brainfuck.hpp"

using emscripten::val;

// ==========================================
// 最小のテストハーネス
// ==========================================

static int g_failures = 0;
static int g_checks = 0;
static std::string g_currentTest;

#define CHECK(cond)                                                        \
    do {                                                                   \
        g_checks++;                                                        \
        if (!(cond)) {                                                     \
            g_failures++;                                                  \
            std::cerr << "  FAIL [" << g_currentTest << ":" << __LINE__    \
                      << "] " << #cond << std::endl;                       \
        }                                                                  \
    } while (0)

#define CHECK_EQ(actual, expected)                                         \
    do {                                                                   \
        g_checks++;                                                        \
        if (!((actual) == (expected))) {                                   \
            g_failures++;                                                  \
            std::cerr << "  FAIL [" << g_currentTest << ":" << __LINE__    \
                      << "] " << #actual << std::endl                      \
                      << "    expected: " << (expected) << std::endl       \
                      << "    actual  : " << (actual) << std::endl;        \
        }                                                                  \
    } while (0)

static void beginTest(const std::string& name) {
    g_currentTest = name;
    std::cout << "- " << name << std::endl;
}

// ==========================================
// 観測用ヘルパー
// ==========================================

// 公開APIだけを使って可視状態のスナップショットを取る。
// private メンバを覗かないので、実装を変えてもテストは壊れない。
struct Fingerprint {
    int ptr = 0;
    int pc = 0;
    bool isError = false;
    std::string errorMessage;
    std::string output;
    std::vector<int> tape;

    bool operator==(const Fingerprint& o) const {
        return ptr == o.ptr && pc == o.pc && isError == o.isError &&
               errorMessage == o.errorMessage && output == o.output &&
               tape == o.tape;
    }
};

static Fingerprint fingerprint(Brainfuck& bf, int start = 0, int range = 40) {
    val params = val::object();
    params.set("start", start);
    params.set("range", range);
    val s = bf.getState(params);

    Fingerprint f;
    f.ptr = s["ptr"].as<int>();
    f.pc = s["pc"].as<int>();
    f.isError = s["isError"].as<bool>();
    f.errorMessage = s["errorMessage"].as<std::string>();
    f.output = bf.getOutput();

    val tape = s["tape"];
    int n = tape["length"].as<int>();
    f.tape.reserve(n);
    for (int i = 0; i < n; i++) {
        f.tape.push_back(tape[i]["value"].as<int>());
    }
    return f;
}

static void describe(const char* label, const Fingerprint& f) {
    std::cerr << "    " << label << ": ptr=" << f.ptr << " pc=" << f.pc
              << " err=" << (f.isError ? "1" : "0")
              << " out=\"" << f.output << "\"" << std::endl;
}

// step() を上限まで繰り返す。上限は暴走プログラムでテストが固まらないための保険。
// step() は1回ごとに 30000 バイトのテープを丸ごと複製して履歴に積むため、
// ここを大きくするとテスト自体が現実的な時間で終わらなくなる。
static long long stepUntilHalt(Brainfuck& bf, long long limit = 100000) {
    long long n = 0;
    while (n < limit && bf.step()) n++;
    return n;
}

// ==========================================
// テスト1: step() の繰り返しと runToEnd() が完全に一致する
//
// これが最重要。step() と runToEnd() は現在70行の重複コードで、
// 1つの実行関数へ統合する予定。このテストがその安全網になる。
// ==========================================

static void testStepRunToEndEquivalence() {
    beginTest("step() の繰り返し == runToEnd()");

    struct Case { const char* name; const char* code; const char* input; };
    const Case cases[] = {
        {"HelloWorld",
         "++++++++++[>+++++++>++++++++++>+++>+<<<<-]>++.>+.+++++++..+++.>++."
         "<<+++++++++++++++.>.+++.------.--------.>+.>.", ""},
        {"単純ループ",     "+++[>++<-]>.",        ""},
        // EOF が -1 なので ,[.,] は停止しない。回数を固定した cat を使う。
        {"cat",           ",.,.,.",              "abc"},
        {"入力が尽きる",   ",,,.",                "a"},
        {"空プログラム",   "",                    ""},
        {"命令以外のみ",   "hello world",         ""},
        {"左端で範囲外",   "<",                   ""},
        {"ネストしたループ", "++[>++[>+<-]<-]>>.", ""},
        {"変数宣言",      "!Var1!+++.",          ""},
        {"変数宣言が未閉じ", "+!Var",             ""},
        {"対応しない閉じ括弧", "+]+",             ""},
        {"対応しない開き括弧", "+[+",             ""},
    };

    for (const auto& c : cases) {
        for (bool mod256 : {true, false}) {
            Brainfuck a, b;
            a.setBrainfuckModint(mod256);
            b.setBrainfuckModint(mod256);
            a.load(c.code, c.input);
            b.load(c.code, c.input);

            stepUntilHalt(a);
            b.runToEnd();

            Fingerprint fa = fingerprint(a);
            Fingerprint fb = fingerprint(b);
            g_checks++;
            if (!(fa == fb)) {
                g_failures++;
                std::cerr << "  FAIL [" << g_currentTest << "] " << c.name
                          << " (mod" << (mod256 ? 256 : 128) << ")" << std::endl;
                describe("step()   ", fa);
                describe("runToEnd()", fb);
            }
        }
    }
}

// ==========================================
// テスト2: 基本的な言語仕様
// ==========================================

static void testHelloWorld() {
    beginTest("Hello World が正しく出力される");
    Brainfuck bf;
    bf.load("++++++++++[>+++++++>++++++++++>+++>+<<<<-]>++.>+.+++++++..+++.>++."
            "<<+++++++++++++++.>.+++.------.--------.>+.>.", "");
    bf.runToEnd();
    CHECK_EQ(bf.getOutput(), std::string("Hello World!\n"));
}

static void testJumpTable() {
    beginTest("[ ] の対応付け");

    // 値が0のセルで [ に入ると、対応する ] の先へ飛ぶ
    {
        Brainfuck bf;
        bf.load("[+++].", "");
        bf.runToEnd();
        Fingerprint f = fingerprint(bf);
        CHECK_EQ(f.tape[0], 0);            // ループ本体は一度も実行されない
        CHECK_EQ(f.output, std::string(1, '\0'));
    }

    // ネストしたループが正しく対応する: 2 * (2 * 1) = 4
    {
        Brainfuck bf;
        bf.load("++[>++[>+<-]<-]", "");
        bf.runToEnd();
        Fingerprint f = fingerprint(bf);
        CHECK_EQ(f.tape[2], 4);
    }

    // 単純なカウントダウンループが停止する
    {
        Brainfuck bf;
        bf.load("+++++[>+<-]", "");
        bf.runToEnd();
        Fingerprint f = fingerprint(bf);
        CHECK_EQ(f.tape[0], 0);
        CHECK_EQ(f.tape[1], 5);
    }
}

static void testEofIsMinusOne() {
    beginTest("EOF は -1 (mod256 なら 255、mod128 なら 127)");

    {
        Brainfuck bf;
        bf.setBrainfuckModint(true);
        bf.load(",", "");
        bf.runToEnd();
        CHECK_EQ(fingerprint(bf).tape[0], 255);
    }
    {
        Brainfuck bf;
        bf.setBrainfuckModint(false);
        bf.load(",", "");
        bf.runToEnd();
        CHECK_EQ(fingerprint(bf).tape[0], 127);
    }
    // 入力が尽きた後の , も EOF になる
    {
        Brainfuck bf;
        bf.setBrainfuckModint(true);
        bf.load(",>,", "a");
        bf.runToEnd();
        Fingerprint f = fingerprint(bf);
        CHECK_EQ(f.tape[0], 'a');
        CHECK_EQ(f.tape[1], 255);
    }
}

static void testCellWrapAround() {
    beginTest("セルの値が mod で循環する");

    // mod256: 0 - 1 = 255
    {
        Brainfuck bf;
        bf.setBrainfuckModint(true);
        bf.load("-", "");
        bf.runToEnd();
        CHECK_EQ(fingerprint(bf).tape[0], 255);
    }
    // mod128: 0 - 1 = 127
    {
        Brainfuck bf;
        bf.setBrainfuckModint(false);
        bf.load("-", "");
        bf.runToEnd();
        CHECK_EQ(fingerprint(bf).tape[0], 127);
    }
    // mod128: 127 + 1 = 0
    {
        Brainfuck bf;
        bf.setBrainfuckModint(false);
        bf.load(std::string(128, '+'), "");
        bf.runToEnd();
        CHECK_EQ(fingerprint(bf).tape[0], 0);
    }
}

// ==========================================
// テスト3: テープ範囲外アクセス
//
// エラー時に pc が「エラーを起こした命令そのもの」を指していること。
// 現在は範囲外エラーの分岐で余計な --pc をしており、pc が1文字手前を指す。
// さらに pc は size_t なので pc==0 での --pc は SIZE_MAX に巻き戻る。
// ==========================================

static void testOutOfRangeLeft() {
    beginTest("左端より左へ移動するとエラー");
    Brainfuck bf;
    bf.load("<", "");
    bool alive = bf.step();

    Fingerprint f = fingerprint(bf);
    CHECK(!alive);
    CHECK(f.isError);
    CHECK_EQ(f.ptr, 0);
    // エラーを起こした '<' 自身を指す。--pc されると size_t が巻き戻って壊れる。
    CHECK_EQ(f.pc, 0);
}

static void testOutOfRangeRight() {
    beginTest("右端より右へ移動するとエラー");
    Brainfuck bf;
    // テープは 30000 セル。30000 回 '>' すると最後の1回で範囲外になる。
    bf.load(std::string(30000, '>'), "");
    bf.runToEnd();

    Fingerprint f = fingerprint(bf, 29990, 12);
    CHECK(f.isError);
    CHECK_EQ(f.ptr, 29999);
    // 30000番目の '>' (0-indexed で 29999) でエラーになる
    CHECK_EQ(f.pc, 29999);
}

// ==========================================
// テスト4: stepBack の往復
// ==========================================

static void testStepBackRoundTrip() {
    beginTest("stepBack で元の状態に完全に戻る");

    Brainfuck bf;
    bf.load("++++++++++[>+++++++<-]>.", "");

    Fingerprint initial = fingerprint(bf);

    // 30ステップ進めて、そのたびの状態を覚えておく
    std::vector<Fingerprint> forward;
    for (int i = 0; i < 30; i++) {
        forward.push_back(fingerprint(bf));
        if (!bf.step()) break;
    }

    // 同じ回数だけ戻ると、各時点の状態と一致するはず
    for (int i = (int)forward.size() - 1; i >= 0; i--) {
        bf.stepBack();
        Fingerprint back = fingerprint(bf);
        g_checks++;
        if (!(back == forward[i])) {
            g_failures++;
            std::cerr << "  FAIL [" << g_currentTest << "] step " << i
                      << " へ戻った状態が一致しない" << std::endl;
            describe("expected", forward[i]);
            describe("actual  ", back);
        }
    }

    CHECK(fingerprint(bf) == initial);
}

static void testStepBackOnEmptyHistory() {
    beginTest("履歴が空でも stepBack が壊れない");
    Brainfuck bf;
    bf.load("+.", "");
    bf.stepBack();
    bf.stepBack();
    Fingerprint f = fingerprint(bf);
    CHECK_EQ(f.pc, 0);
    CHECK_EQ(f.ptr, 0);
    CHECK(!f.isError);
}

static void testStepBackAfterRunToEnd() {
    beginTest("runToEnd の直後に stepBack すると実行前へ戻る");

    Brainfuck bf;
    bf.load("+++.", "");
    Fingerprint before = fingerprint(bf);

    bf.runToEnd();
    Fingerprint after = fingerprint(bf);
    CHECK(!(after == before));  // 実行して状態が変わっている

    // 「戻る」を1回押したら、見た目が変わらなければならない。
    // 最終状態を履歴に積んでいると、1回目の戻るが何も起きないように見える。
    bf.stepBack();
    Fingerprint back = fingerprint(bf);
    g_checks++;
    if (!(back == before)) {
        g_failures++;
        std::cerr << "  FAIL [" << g_currentTest
                  << "] 実行前の状態に戻っていない" << std::endl;
        describe("expected", before);
        describe("actual  ", back);
    }
}

// ==========================================
// テスト5: runToEnd のステップ上限
//
// WASM はメインスレッドで同期実行されるため、停止しないプログラムは
// タブごと固める。上限に達したら中断して制御を返さなければならない。
// ==========================================

static void testRunToEndTerminatesOnInfiniteLoop() {
    beginTest("停止しないプログラムでも runToEnd が返ってくる");

    Brainfuck bf;
    bf.load("+[]", "");  // セルが1のまま無限ループ
    bf.runToEnd();       // 上限が無いとここで永久に返ってこない

    // ここへ到達できた時点で上限が効いている
    CHECK(true);

    val params = val::object();
    params.set("start", 0);
    params.set("range", 4);
    val s = bf.getState(params);
    CHECK(s.hasOwnProperty("interrupted"));
    if (s.hasOwnProperty("interrupted")) {
        CHECK(s["interrupted"].as<bool>());
    }
}

static void testInterruptedClearsOnStep() {
    beginTest("中断フラグは step / stepBack で消える");

    val params = val::object();
    params.set("start", 0);
    params.set("range", 4);

    Brainfuck bf;
    bf.load("+[]", "");
    bf.runToEnd();
    CHECK(bf.getState(params)["interrupted"].as<bool>());

    bf.step();
    CHECK(!bf.getState(params)["interrupted"].as<bool>());

    bf.runToEnd();
    CHECK(bf.getState(params)["interrupted"].as<bool>());

    bf.stepBack();
    CHECK(!bf.getState(params)["interrupted"].as<bool>());
}

static void testRunToEndNotInterruptedNormally() {
    beginTest("正常に停止するプログラムは中断扱いにならない");

    Brainfuck bf;
    bf.load("+++.", "");
    bf.runToEnd();

    val params = val::object();
    params.set("start", 0);
    params.set("range", 4);
    val s = bf.getState(params);
    if (s.hasOwnProperty("interrupted")) {
        CHECK(!s["interrupted"].as<bool>());
    }
}

// ==========================================
// テスト6: load でエンジンが初期状態に戻る
// ==========================================

static void testLoadResetsState() {
    beginTest("load で状態が完全に初期化される");

    Brainfuck bf;
    bf.load("+++++>+++++.", "");
    bf.runToEnd();
    CHECK(fingerprint(bf).tape[0] != 0);

    bf.load("+++++>+++++.", "");
    Fingerprint f = fingerprint(bf);
    CHECK_EQ(f.ptr, 0);
    CHECK_EQ(f.pc, 0);
    CHECK_EQ(f.output, std::string(""));
    CHECK(!f.isError);
    for (int v : f.tape) CHECK_EQ(v, 0);
}

static void testLoadSkipsToFirstCommand() {
    beginTest("load 直後の pc が最初の有効な命令を指す");
    Brainfuck bf;
    bf.load("これはコメント+.", "");
    Fingerprint f = fingerprint(bf);
    // 先頭の非命令文字は読み飛ばされ、'+' の位置を指す
    CHECK(f.pc > 0);
    CHECK_EQ(std::string("これはコメント").size(), (size_t)f.pc);
}

// ==========================================
// テスト7: getState の表示範囲
// ==========================================

static void testGetStateWindow() {
    beginTest("getState がテープの指定範囲だけを返す");

    Brainfuck bf;
    bf.load("", "");

    val params = val::object();
    params.set("start", -3);
    params.set("range", 10);
    val s = bf.getState(params);
    val tape = s["tape"];

    CHECK_EQ(tape["length"].as<int>(), 10);
    // 負の番地は存在しないセルとして返る
    CHECK(!tape[0]["exists"].as<bool>());
    CHECK(!tape[2]["exists"].as<bool>());
    CHECK(tape[3]["exists"].as<bool>());
    CHECK_EQ(tape[3]["index"].as<int>(), 0);
}

// ==========================================

int main() {
    std::cout << "=== AlgoViz core tests ===" << std::endl;

    testStepRunToEndEquivalence();
    testHelloWorld();
    testJumpTable();
    testEofIsMinusOne();
    testCellWrapAround();
    testOutOfRangeLeft();
    testOutOfRangeRight();
    testStepBackRoundTrip();
    testStepBackOnEmptyHistory();
    testStepBackAfterRunToEnd();
    testRunToEndTerminatesOnInfiniteLoop();
    testInterruptedClearsOnStep();
    testRunToEndNotInterruptedNormally();
    testLoadResetsState();
    testLoadSkipsToFirstCommand();
    testGetStateWindow();

    std::cout << std::endl;
    if (g_failures == 0) {
        std::cout << "OK: " << g_checks << " checks passed" << std::endl;
        return 0;
    }
    std::cout << "FAILED: " << g_failures << " / " << g_checks
              << " checks failed" << std::endl;
    return 1;
}
