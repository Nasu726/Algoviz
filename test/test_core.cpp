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

#include <cmath>
#include <set>
#include <map>
#include <tuple>
#include <array>
#include <algorithm>

#include "../cpp/include/Brainfuck.hpp"
#include "../cpp/include/GraphVisualizer.hpp"
#include "../cpp/include/TreeLayout.hpp"
#include "../cpp/include/AutomatonVisualizer.hpp"
#include "../cpp/include/TraversalVisualizer.hpp"
#include "../cpp/include/BstVisualizer.hpp"
#include "../cpp/include/HeapVisualizer.hpp"
#include "../cpp/include/TrieVisualizer.hpp"
#include "../cpp/include/HuffmanVisualizer.hpp"
#include "../cpp/include/AvlVisualizer.hpp"

using emscripten::val;

// ==========================================
// 最小のテストハーネス
// ==========================================

// 成功したテストは何も出さない。読む必要があるのは失敗したときだけで、
// 全部の名前を並べても行数が増えるだけになる。
// どこで落ちたか追いたいときは --verbose を付けると進行が見える。

static int g_failures = 0;
static int g_checks = 0;
static std::string g_currentTest;
static bool g_currentTestReported = false;
static bool g_verbose = false;

// 失敗したテストの名前を1回だけ出す
static void failHeader() {
    if (g_currentTestReported) return;
    g_currentTestReported = true;
    std::cerr << std::endl << "FAIL: " << g_currentTest << std::endl;
}

// 手書きの比較が失敗したときに呼ぶ
static void reportFailure(const std::string& detail) {
    g_failures++;
    failHeader();
    std::cerr << "  " << detail << std::endl;
}

#define CHECK(cond)                                                        \
    do {                                                                   \
        g_checks++;                                                        \
        if (!(cond)) {                                                     \
            g_failures++;                                                  \
            failHeader();                                                  \
            std::cerr << "  L" << __LINE__ << ": " << #cond << std::endl;  \
        }                                                                  \
    } while (0)

#define CHECK_EQ(actual, expected)                                         \
    do {                                                                   \
        g_checks++;                                                        \
        if (!((actual) == (expected))) {                                   \
            g_failures++;                                                  \
            failHeader();                                                  \
            std::cerr << "  L" << __LINE__ << ": " << #actual << std::endl \
                      << "    expected: " << (expected) << std::endl       \
                      << "    actual  : " << (actual) << std::endl;        \
        }                                                                  \
    } while (0)

#define CHECK_NEAR(actual, expected, tol)                                  \
    do {                                                                   \
        g_checks++;                                                        \
        if (!(std::fabs((actual) - (expected)) <= (tol))) {                \
            g_failures++;                                                  \
            failHeader();                                                  \
            std::cerr << "  L" << __LINE__ << ": " << #actual << std::endl \
                      << "    expected: " << (expected) << " (許容 " << (tol) << ")" << std::endl \
                      << "    actual  : " << (actual) << std::endl;        \
        }                                                                  \
    } while (0)

static void beginTest(const std::string& name) {
    g_currentTest = name;
    g_currentTestReported = false;
    if (g_verbose) std::cout << "- " << name << std::endl;
}

static void beginSection(const char* name) {
    if (g_verbose) std::cout << std::endl << "=== " << name << " ===" << std::endl;
}

// ==========================================
// 観測用ヘルパー
// ==========================================

// 進行状況は要求したときだけ組み立てられる
static val progressParams() {
    val p = val::object();
    p.set("withProgress", true);
    return p;
}

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
                reportFailure(std::string(c.name) + (mod256 ? " (mod256)" : " (mod128)"));
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
            reportFailure("step " + std::to_string(i) + " へ戻った状態が一致しない");
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
        reportFailure("実行前の状態に戻っていない");
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
// グラフ: 観測用ヘルパー
// ==========================================

struct ParsedGraph {
    int v = 0;
    int declaredE = 0;
    std::vector<std::pair<int, int>> edges;
    std::vector<float> weights;
    std::vector<float> xs, ys;
};

static ParsedGraph readGraph(GraphVisualizer& g) {
    val params = val::object();
    params.set("withText", true);
    val state = g.getState(params);

    ParsedGraph pg;
    std::istringstream iss(state["graphText"].as<std::string>());
    std::string line;
    iss >> pg.v >> pg.declaredE;
    std::getline(iss, line); // "V E" の行の残り

    // 辺の行は重み付きなら3列、重み無しなら2列。
    // 頂点の重み行 (V 個の数値) が挟まることもあるので、列数で見分ける。
    while ((int)pg.edges.size() < pg.declaredE && std::getline(iss, line)) {
        std::istringstream ls(line);
        std::vector<float> nums;
        float x;
        while (ls >> x) nums.push_back(x);
        if (nums.size() < 2 || nums.size() > 3) continue;
        pg.edges.push_back({(int)nums[0], (int)nums[1]});
        pg.weights.push_back(nums.size() == 3 ? nums[2] : 1.0f);
    }

    val nodes = state["nodes"];
    int len = nodes["length"].as<int>();
    for (int i = 0; i + 3 < len; i += 4) {
        pg.xs.push_back(nodes[i].as<float>());
        pg.ys.push_back(nodes[i + 1].as<float>());
    }
    return pg;
}

// ==========================================
// グラフ生成の不変条件
//
// 乱数は random_device 由来なので、1回の生成ではなく多数回まわして
// 「どの生成結果でも成り立つ性質」を確かめる。
// ==========================================

static void testNoSelfLoopWhenDisallowed() {
    beginTest("自己ループを許さない設定なら自己ループが無い");
    for (int trial = 0; trial < 30; trial++) {
        GraphVisualizer g;
        g.load("horizontal", "random 8 20 1 0 1 0"); // selfLoop=0, sameEdge=1
        for (auto& e : readGraph(g).edges) {
            if (e.first == e.second) {
                g_checks++;
                reportFailure("自己ループ " + std::to_string(e.first));
                return;
            }
        }
    }
    CHECK(true);
}

static void testSelfLoopAppearsWhenAllowed() {
    beginTest("自己ループを許す設定なら実際に出現しうる");
    bool seen = false;
    for (int trial = 0; trial < 60 && !seen; trial++) {
        GraphVisualizer g;
        g.load("horizontal", "random 4 30 1 1 1 0"); // selfLoop=1, sameEdge=1
        for (auto& e : readGraph(g).edges) if (e.first == e.second) seen = true;
    }
    CHECK(seen);
}

static void testNoMultiEdgeWhenDisallowed() {
    beginTest("多重辺を許さない設定なら辺が重複しない");
    for (int trial = 0; trial < 30; trial++) {
        // 無向: (min,max) で正規化して重複を見る
        {
            GraphVisualizer g;
            g.load("horizontal", "random 7 40 1 1 0 0"); // sameEdge=0, 無向
            std::set<std::pair<int, int>> seen;
            for (auto& e : readGraph(g).edges) {
                auto key = std::make_pair(std::min(e.first, e.second), std::max(e.first, e.second));
                if (!seen.insert(key).second) {
                    g_checks++;
                    reportFailure("無向で重複 " + std::to_string(key.first) + "-" + std::to_string(key.second));
                    return;
                }
            }
        }
        // 有向: 順序付きの組で重複を見る
        {
            GraphVisualizer g;
            g.load("horizontal", "random 7 60 1 1 0 1"); // sameEdge=0, 有向
            std::set<std::pair<int, int>> seen;
            for (auto& e : readGraph(g).edges) {
                if (!seen.insert(e).second) {
                    g_checks++;
                    reportFailure("有向で重複 " + std::to_string(e.first) + "->" + std::to_string(e.second));
                    return;
                }
            }
        }
    }
    CHECK(true);
}

static void testEdgeCountNeverExceedsRequest() {
    beginTest("多重辺を許さないとき辺数は要求値と可能な組合せ数以下");
    GraphVisualizer g;
    // 無向・自己ループ無しの V=4 なら組合せは 6 通りしかない
    g.load("horizontal", "random 4 100 1 0 0 0");
    ParsedGraph pg = readGraph(g);
    CHECK_EQ(pg.v, 4);
    CHECK((int)pg.edges.size() <= 6);
}

static void testCompleteGraphEdgeCount() {
    beginTest("完全グラフの辺数");

    {   // 無向: V(V-1)/2
        GraphVisualizer g;
        g.load("horizontal", "complete 6 1 0");
        ParsedGraph pg = readGraph(g);
        CHECK_EQ(pg.v, 6);
        CHECK_EQ((int)pg.edges.size(), 15);
    }
    {   // 有向: V(V-1)
        GraphVisualizer g;
        g.load("horizontal", "complete 6 1 1");
        ParsedGraph pg = readGraph(g);
        CHECK_EQ(pg.v, 6);
        CHECK_EQ((int)pg.edges.size(), 30);
    }
    {   // V=1 でも壊れない
        GraphVisualizer g;
        g.load("horizontal", "complete 1 1 0");
        ParsedGraph pg = readGraph(g);
        CHECK_EQ(pg.v, 1);
        CHECK_EQ((int)pg.edges.size(), 0);
    }
}

static void testNodeCountIsClamped() {
    beginTest("頂点数の上限は C++ 側で守られる");

    {
        GraphVisualizer g;
        g.load("horizontal", "random 500 10 1 0 0 0");
        CHECK_EQ(readGraph(g).v, GraphVisualizer::MAX_NODES);
    }
    {   // テキスト入力経由でも同じ
        GraphVisualizer g;
        g.load("horizontal", "custom 1 0 0 1\n500 0\n");
        CHECK_EQ(readGraph(g).v, GraphVisualizer::MAX_NODES);
    }
    {
        GraphVisualizer g;
        g.load("horizontal", "complete 500 1 0");
        CHECK_EQ(readGraph(g).v, GraphVisualizer::MAX_NODES);
    }
}

// ==========================================
// テキスト入力のパース
// ==========================================

static void testCustomGraphParsing() {
    beginTest("テキストからグラフを生成する");

    GraphVisualizer g;
    g.load("horizontal", "custom 1 0 0 1\n4 3\n0 1 5\n1 2 7\n2 3 9\n");
    ParsedGraph pg = readGraph(g);

    CHECK_EQ(pg.v, 4);
    CHECK_EQ((int)pg.edges.size(), 3);
    if (pg.edges.size() == 3) {
        CHECK(pg.edges[0] == std::make_pair(0, 1));
        CHECK(pg.edges[2] == std::make_pair(2, 3));
        CHECK_EQ(pg.weights[1], 7.0f);
    }
}

static void testCustomGraphOptionalWeight() {
    beginTest("重みを省略した辺は 1 として扱う");

    GraphVisualizer g;
    g.load("horizontal", "custom 1 0 0 1\n3 2\n0 1\n1 2\n");
    ParsedGraph pg = readGraph(g);

    CHECK_EQ(pg.v, 3);
    CHECK_EQ((int)pg.edges.size(), 2);
    // 重み無しグラフとして 1 になる。こうするとダイクストラの結果が
    // 幅優先探索と一致して読みやすい。
    if (pg.weights.size() == 2) {
        CHECK_EQ(pg.weights[0], 1.0f);
        CHECK_EQ(pg.weights[1], 1.0f);
    }
}

static void testCustomGraphRejectsOutOfRangeVertices() {
    beginTest("範囲外の頂点番号を含む辺は捨てられる");

    // ここで弾かないと隣接リスト構築で範囲外アクセスになる
    GraphVisualizer g;
    g.load("horizontal", "custom 1 0 0 1\n3 4\n0 1\n1 99\n-5 2\n2 0\n");
    ParsedGraph pg = readGraph(g);

    CHECK_EQ(pg.v, 3);
    CHECK_EQ((int)pg.edges.size(), 2);
    for (auto& e : pg.edges) {
        CHECK(e.first >= 0 && e.first < 3);
        CHECK(e.second >= 0 && e.second < 3);
    }
}

static void testCustomGraphIgnoresJunkLines() {
    beginTest("空行やゴミ行があっても壊れない");

    GraphVisualizer g;
    g.load("horizontal", "custom 1 0 0 1\n3 2\n\n0 1 3\n\nhello\n1 2 4\n");
    ParsedGraph pg = readGraph(g);
    CHECK_EQ(pg.v, 3);
    CHECK_EQ((int)pg.edges.size(), 2);
}

// ==========================================
// レイアウト
// ==========================================

static void testLayoutProducesFiniteCoordinates() {
    beginTest("レイアウトが収束し、座標が NaN にならない");

    const char* cases[] = {
        "random 12 18 1 0 0 0",
        "random 30 40 1 1 1 1",
        "complete 10 1 0",
        "custom 1 0 0 1\n6 2\n0 1\n2 3\n",   // 非連結（孤立点あり）
        "custom 1 0 0 1\n1 0\n",             // 頂点1個
        "custom 1 0 0 1\n2 1\n0 1\n",        // 直線
        "custom 1 0 0 1\n5 0\n",             // 全部孤立
    };

    for (const char* c : cases) {
        GraphVisualizer g;
        g.load("horizontal", c);
        CHECK(g.prepare()); // skipExtension=1 なので一気に収束する

        ParsedGraph pg = readGraph(g);
        bool ok = true;
        for (size_t i = 0; i < pg.xs.size(); i++) {
            if (!std::isfinite(pg.xs[i]) || !std::isfinite(pg.ys[i])) ok = false;
        }
        g_checks++;
        if (!ok) reportFailure(std::string("座標が有限でない: ") + c);
    }
}

static void testLayoutDoesNotCollapseNodes() {
    beginTest("レイアウト後にノードが重ならない");

    GraphVisualizer g;
    g.load("horizontal", "random 20 30 1 0 0 0");
    g.prepare();

    ParsedGraph pg = readGraph(g);
    float worst = 1e9f;
    for (size_t i = 0; i < pg.xs.size(); i++) {
        for (size_t j = i + 1; j < pg.xs.size(); j++) {
            float dx = pg.xs[i] - pg.xs[j];
            float dy = pg.ys[i] - pg.ys[j];
            worst = std::min(worst, std::sqrt(dx * dx + dy * dy));
        }
    }
    // ノード半径は 20 なので、10px 未満まで寄っていたら描画が潰れている
    g_checks++;
    if (worst < 10.0f) reportFailure("最接近距離 " + std::to_string(worst));
}

static void testPrepareIsIdempotentOnceStable() {
    beginTest("収束後に prepare を繰り返しても座標が動かない");

    GraphVisualizer g;
    g.load("horizontal", "random 10 14 1 0 0 0");
    g.prepare();
    ParsedGraph a = readGraph(g);

    for (int i = 0; i < 5; i++) CHECK(g.prepare());
    ParsedGraph b = readGraph(g);

    bool same = a.xs.size() == b.xs.size();
    for (size_t i = 0; same && i < a.xs.size(); i++) {
        if (a.xs[i] != b.xs[i] || a.ys[i] != b.ys[i]) same = false;
    }
    CHECK(same);
}

// ==========================================
// 基底クラスとしての振る舞い
// ==========================================

static void testGraphHasNoAlgorithmStep() {
    beginTest("基底クラスは進めるアルゴリズムを持たない");
    GraphVisualizer g;
    CHECK(!g.step());
    g.runToEnd();   // 無限ループしないこと
    CHECK(true);
}

static void testGraphTextOnlyWhenRequested() {
    beginTest("graphText は要求したときだけ作られる");

    GraphVisualizer g;
    g.load("horizontal", "complete 5 1 0");

    CHECK(!g.getState(progressParams()).hasOwnProperty("graphText"));

    val params = val::object();
    params.set("withText", true);
    CHECK(g.getState(params).hasOwnProperty("graphText"));
}

// ==========================================
// 色チャンネル
//
// アルゴリズムの可視化はノードと辺の colorId だけで表現する。
// C++ で塗った色が JS 側の配列に載ることを確かめる。
// ==========================================

// protected な graph を触るためのテスト用サブクラス
class ColorProbe : public GraphVisualizer {
public:
    void paintNode(int i, int c) { graph->setNodeColor(i, c); }
    void paintEdge(int i, int c) { graph->setEdgeColor(i, c); }
    void clearColors()           { graph->resetColors(); }
    int  readNode(int i) const   { return graph->nodeColor(i); }
    int  readEdge(int i) const   { return graph->edgeColor(i); }
};

static void testColorChannelReachesTheView() {
    beginTest("塗った色が JS へ渡す配列に載る");

    ColorProbe g;
    g.load("horizontal", "custom 1 0 0 1\n4 3\n0 1\n1 2\n2 3\n");

    g.paintNode(0, NODE_START);
    g.paintNode(1, NODE_FRONTIER);
    g.paintEdge(0, EDGE_PATH);
    g.paintEdge(2, EDGE_TREE);

    val state = g.getState(progressParams());
    val nodes = state["nodes"];
    val edges = state["edges"];

    // nodeData = [x, y, weight, colorId] * V
    CHECK_EQ((int)nodes[3].as<float>(), (int)NODE_START);
    CHECK_EQ((int)nodes[7].as<float>(), (int)NODE_FRONTIER);
    CHECK_EQ((int)nodes[11].as<float>(), (int)NODE_DEFAULT);

    // edgeData = [from, to, weight, colorId] * E
    CHECK_EQ((int)edges[3].as<float>(), (int)EDGE_PATH);
    CHECK_EQ((int)edges[7].as<float>(), (int)EDGE_DEFAULT);
    CHECK_EQ((int)edges[11].as<float>(), (int)EDGE_TREE);
}

static void testResetColors() {
    beginTest("resetColors で全部が既定色に戻る");

    ColorProbe g;
    g.load("horizontal", "custom 1 0 0 1\n3 2\n0 1\n1 2\n");
    g.paintNode(0, NODE_PATH);
    g.paintNode(2, NODE_VISITED);
    g.paintEdge(1, EDGE_ACTIVE);

    g.clearColors();
    for (int i = 0; i < 3; i++) CHECK_EQ(g.readNode(i), (int)NODE_DEFAULT);
    for (int i = 0; i < 2; i++) CHECK_EQ(g.readEdge(i), (int)EDGE_DEFAULT);
}

static void testColorSettersIgnoreOutOfRange() {
    beginTest("範囲外の添字に色を塗っても壊れない");

    ColorProbe g;
    g.load("horizontal", "custom 1 0 0 1\n2 1\n0 1\n");
    g.paintNode(-1, NODE_PATH);
    g.paintNode(99, NODE_PATH);
    g.paintEdge(-1, EDGE_PATH);
    g.paintEdge(99, EDGE_PATH);

    CHECK_EQ(g.readNode(0), (int)NODE_DEFAULT);
    CHECK_EQ(g.readEdge(0), (int)EDGE_DEFAULT);
}

static void testGenerationChangesOnRebuild() {
    beginTest("グラフを作り直すと generation が進む");

    GraphVisualizer g;
    int before = g.getState(progressParams())["generation"].as<int>();

    g.load("horizontal", "random 6 8 1 0 0 0");
    int after = g.getState(progressParams())["generation"].as<int>();
    CHECK(after > before);

    // レイアウトの向きを変えただけでは作り直さない
    g.load("vertical", "");
    CHECK_EQ(g.getState(progressParams())["generation"].as<int>(), after);
}

// ==========================================
// オートマトン
// ==========================================

static void testAutomatonIsAlwaysDirected() {
    beginTest("オートマトンは無向を指定しても有向になる");

    AutomatonVisualizer a;
    a.load("horizontal", "complete 5 1 0"); // dir=0 を指定
    ParsedGraph pg = readGraph(a);
    // 有向完全グラフなので V(V-1) = 20 本
    CHECK_EQ((int)pg.edges.size(), 20);
    CHECK(a.getState(progressParams())["isDirected"].as<bool>());
    CHECK(a.getState(progressParams())["isAutomaton"].as<bool>());
}

static void testAutomatonStartAndAcceptingStates() {
    beginTest("初期状態と受理状態を C++ が保持する");

    AutomatonVisualizer a;
    a.load("horizontal", "custom 1 0 0 1\n5 2\n0 1\n1 2\n");

    a.load("setStartNode", "2");
    CHECK_EQ(a.getState(progressParams())["startNodeIndex"].as<int>(), 2);

    // 範囲外は -1 に落とす
    a.load("setStartNode", "99");
    CHECK_EQ(a.getState(progressParams())["startNodeIndex"].as<int>(), -1);

    // カンマ区切りを受け付け、範囲外は捨てる
    a.load("setAccepting", "1, 3, 99");
    val accepting = a.getState(progressParams())["acceptingStates"];
    CHECK_EQ(accepting["length"].as<int>(), 2);
    if (accepting["length"].as<int>() == 2) {
        CHECK_EQ(accepting[0].as<int>(), 1);
        CHECK_EQ(accepting[1].as<int>(), 3);
    }
}

static void testAutomatonDropsStaleStatesOnRegenerate() {
    beginTest("グラフを作り直すと範囲外になった状態指定が消える");

    AutomatonVisualizer a;
    a.load("horizontal", "custom 1 0 0 1\n10 0\n");
    a.load("setStartNode", "8");
    a.load("setAccepting", "7, 9");
    CHECK_EQ(a.getState(progressParams())["startNodeIndex"].as<int>(), 8);

    // 3頂点に作り直すと 7,8,9 は存在しなくなる
    a.load("horizontal", "custom 1 0 0 1\n3 0\n");
    CHECK_EQ(a.getState(progressParams())["startNodeIndex"].as<int>(), -1);
    CHECK_EQ(a.getState(progressParams())["acceptingStates"]["length"].as<int>(), 0);
}

// ==========================================
// BFS / DFS
// ==========================================

// テキスト形式のグラフを参照実装で解くためのヘルパー
struct RefGraph {
    int v = 0;
    std::vector<std::pair<int, int>> edges;
    std::vector<std::vector<int>> adj;

    RefGraph(int v_, std::vector<std::pair<int, int>> es, bool directed) : v(v_), edges(es) {
        adj.assign(v, {});
        for (auto& e : es) {
            adj[e.first].push_back(e.second);
            if (!directed && e.first != e.second) adj[e.second].push_back(e.first);
        }
    }

    // 到達可能な頂点集合（参照実装）
    std::vector<char> reachableFrom(int s) const {
        std::vector<char> seen(v, 0);
        if (s < 0 || s >= v) return seen;
        std::vector<int> stack{s};
        seen[s] = 1;
        while (!stack.empty()) {
            int u = stack.back(); stack.pop_back();
            for (int w : adj[u]) if (!seen[w]) { seen[w] = 1; stack.push_back(w); }
        }
        return seen;
    }

    bool hasEdge(int a, int b, bool directed) const {
        for (auto& e : edges) {
            if (e.first == a && e.second == b) return true;
            if (!directed && e.first == b && e.second == a) return true;
        }
        return false;
    }
};

static std::string traversalCmd(const std::string& mode, int s, int g) {
    std::ostringstream oss;
    oss << mode << " " << s << " " << g;
    return oss.str();
}

// 探索を最後まで進める。
// 探索を完了させた step は false を返すが、その1手も状態を変えている
// (履歴にも積まれる) ので、呼んだ回数をそのまま返す。
static int runTraversal(TraversalVisualizer& t, int limit = 200000) {
    int steps = 0;
    while (steps < limit) {
        bool alive = t.step();
        steps++;
        if (!alive) break;
    }
    return steps;
}

// 比較したい可視状態だけを抜き出す
struct TravRec {
    std::vector<int> order, frontier, path;
    int current = -1;
    bool finished = false, found = false;

    bool operator==(const TravRec& o) const {
        return order == o.order && frontier == o.frontier && path == o.path &&
               current == o.current && finished == o.finished && found == o.found;
    }
};

static std::vector<int> valToVector(val arr) {
    std::vector<int> out;
    int n = arr["length"].as<int>();
    for (int i = 0; i < n; i++) out.push_back(arr[i].as<int>());
    return out;
}

static TravRec readTrav(TraversalVisualizer& t) {
    val s = t.getState(progressParams());
    TravRec r;
    r.order    = valToVector(s["visitOrder"]);
    r.frontier = valToVector(s["frontier"]);
    r.path     = valToVector(s["path"]);
    r.current  = s["current"].as<int>();
    r.finished = s["finished"].as<bool>();
    r.found    = s["found"].as<bool>();
    return r;
}

static void testTraversalVisitsExactlyTheReachableSet() {
    beginTest("到達可能な頂点集合が参照実装と一致する");

    struct Case { const char* text; int v; std::vector<std::pair<int,int>> es; bool directed; };
    const Case cases[] = {
        // 連結
        {"custom 1 0 0 1\n5 4\n0 1\n1 2\n2 3\n3 4\n", 5, {{0,1},{1,2},{2,3},{3,4}}, false},
        // 非連結（4,5 は孤立した別成分）
        {"custom 1 0 0 1\n6 3\n0 1\n1 2\n4 5\n",      6, {{0,1},{1,2},{4,5}},       false},
        // 有向。0 からは 3 に届かない
        {"custom 1 1 0 1\n4 3\n0 1\n1 2\n3 0\n",      4, {{0,1},{1,2},{3,0}},       true},
        // 自己ループと多重辺
        {"custom 1 0 0 1\n3 4\n0 0\n0 1\n0 1\n1 2\n", 3, {{0,0},{0,1},{0,1},{1,2}}, false},
        // 孤立点のみ
        {"custom 1 0 0 1\n3 0\n",                     3, {},                        false},
    };

    for (const auto& c : cases) {
        RefGraph ref(c.v, c.es, c.directed);
        std::vector<char> expected = ref.reachableFrom(0);

        for (const char* mode : {"bfs", "dfs"}) {
            TraversalVisualizer t;
            t.load("horizontal", c.text);
            t.load("setTraversal", traversalCmd(mode, 0, -1));
            runTraversal(t);

            std::vector<int> order = valToVector(t.getState(progressParams())["visitOrder"]);
            std::vector<char> got(c.v, 0);
            for (int u : order) got[u] = 1;

            g_checks++;
            if (got != expected) reportFailure(std::string(mode) + " : " + c.text);
        }
    }
}

static void testTraversalVisitsEachVertexOnce() {
    beginTest("各頂点をちょうど1回だけ訪問する");

    for (const char* mode : {"bfs", "dfs"}) {
        TraversalVisualizer t;
        t.load("horizontal", "custom 1 0 0 1\n8 10\n0 1\n0 2\n1 3\n2 3\n3 4\n4 5\n5 6\n6 7\n7 4\n2 6\n");
        t.load("setTraversal", traversalCmd(mode, 0, -1));
        runTraversal(t);

        std::vector<int> order = valToVector(t.getState(progressParams())["visitOrder"]);
        std::set<int> unique(order.begin(), order.end());
        CHECK_EQ(order.size(), unique.size());
        CHECK_EQ((int)order.size(), 8);
    }
}

static void testBfsFindsShortestPath() {
    beginTest("BFS が見つける経路は最短");

    // 0-1-2-3 の道と 0-3 の近道。BFS なら 0->3 の1辺で着く
    TraversalVisualizer t;
    t.load("horizontal", "custom 1 0 0 1\n4 4\n0 1\n1 2\n2 3\n0 3\n");
    t.load("setTraversal", traversalCmd("bfs", 0, 3));
    runTraversal(t);

    val state = t.getState(progressParams());
    CHECK(state["found"].as<bool>());
    std::vector<int> path = valToVector(state["path"]);
    CHECK_EQ((int)path.size(), 2);
    if (path.size() >= 2) {
        CHECK_EQ(path.front(), 0);
        CHECK_EQ(path.back(), 3);
    }
}

static void testPathIsActuallyWalkable() {
    beginTest("見つかった経路が実際に辺を辿れる");

    struct Case { const char* text; int v; std::vector<std::pair<int,int>> es; bool directed; int s, g; };
    const Case cases[] = {
        {"custom 1 0 0 1\n6 6\n0 1\n1 2\n2 3\n3 4\n4 5\n0 5\n", 6, {{0,1},{1,2},{2,3},{3,4},{4,5},{0,5}}, false, 0, 4},
        {"custom 1 1 0 1\n5 5\n0 1\n1 2\n2 3\n3 4\n0 4\n",      5, {{0,1},{1,2},{2,3},{3,4},{0,4}},       true,  0, 3},
        {"custom 1 0 0 1\n7 6\n0 1\n1 2\n2 3\n3 4\n4 5\n5 6\n", 7, {{0,1},{1,2},{2,3},{3,4},{4,5},{5,6}}, false, 0, 6},
    };

    for (const auto& c : cases) {
        RefGraph ref(c.v, c.es, c.directed);
        for (const char* mode : {"bfs", "dfs"}) {
            TraversalVisualizer t;
            t.load("horizontal", c.text);
            t.load("setTraversal", traversalCmd(mode, c.s, c.g));
            runTraversal(t);

            val state = t.getState(progressParams());
            CHECK(state["found"].as<bool>());
            std::vector<int> path = valToVector(state["path"]);

            g_checks++;
            bool ok = path.size() >= 2 && path.front() == c.s && path.back() == c.g;
            for (size_t i = 0; ok && i + 1 < path.size(); i++) {
                if (!ref.hasEdge(path[i], path[i + 1], c.directed)) ok = false;
            }
            if (!ok) reportFailure(std::string(mode) + " の経路が辿れない: " + c.text);
        }
    }
}

static void testNoPathWhenUnreachable() {
    beginTest("到達できない終点なら経路は見つからない");

    for (const char* mode : {"bfs", "dfs"}) {
        TraversalVisualizer t;
        t.load("horizontal", "custom 1 0 0 1\n5 2\n0 1\n3 4\n");
        t.load("setTraversal", traversalCmd(mode, 0, 4));
        runTraversal(t);

        val state = t.getState(progressParams());
        CHECK(!state["found"].as<bool>());
        CHECK(state["finished"].as<bool>());
        CHECK_EQ(state["path"]["length"].as<int>(), 0);
    }
}

static void testDfsGoesDeepBeforeWide() {
    beginTest("DFS は横に広がる前に深く潜る");

    // 0 から 1 と 4 へ。1 の先は 2 -> 3 と続く。
    // DFS なら 0,1,2,3 と潜ってから 4 に来る。BFS なら 0,1,4 が先。
    const char* text = "custom 1 1 0 1\n5 4\n0 1\n1 2\n2 3\n0 4\n";

    {
        TraversalVisualizer t;
        t.load("horizontal", text);
        t.load("setTraversal", traversalCmd("dfs", 0, -1));
        runTraversal(t);
        std::vector<int> order = valToVector(t.getState(progressParams())["visitOrder"]);
        CHECK_EQ((int)order.size(), 5);
        if (order.size() == 5) {
            CHECK(order[0] == 0 && order[1] == 1 && order[2] == 2 && order[3] == 3 && order[4] == 4);
        }
    }
    {
        TraversalVisualizer t;
        t.load("horizontal", text);
        t.load("setTraversal", traversalCmd("bfs", 0, -1));
        runTraversal(t);
        std::vector<int> order = valToVector(t.getState(progressParams())["visitOrder"]);
        CHECK_EQ((int)order.size(), 5);
        // 0 の隣 (1 と 4) が先に並ぶ
        if (order.size() == 5) {
            CHECK(order[0] == 0 && order[1] == 1 && order[2] == 4);
        }
    }
}

static void testStepCountIsBounded() {
    beginTest("ステップ数が頂点数と辺数で抑えられる");

    // 1ステップ = 「辺を1本見る」か「頂点を1つ処理し終える」なので
    // 概ね V + 2E に収まる (無向は両向きから見るので 2E)
    for (const char* mode : {"bfs", "dfs"}) {
        TraversalVisualizer t;
        t.load("horizontal", "random 20 30 1 0 0 0");
        t.load("setTraversal", traversalCmd(mode, 0, -1));
        int steps = runTraversal(t);
        CHECK(steps <= 20 + 2 * 30 + 5);
    }
}

static void testStepBackReturnsToInitialState() {
    beginTest("stepBack を戻しきると初期状態に一致する");

    for (const char* mode : {"bfs", "dfs"}) {
        TraversalVisualizer t;
        t.load("horizontal", "custom 1 0 0 1\n6 7\n0 1\n0 2\n1 3\n2 3\n3 4\n4 5\n2 5\n");
        t.load("setTraversal", traversalCmd(mode, 0, 5));

        TravRec initial = readTrav(t);
        CHECK(runTraversal(t) > 0);

        // UI の「戻る」と同じように、戻せなくなるまで戻す
        int guard = 0;
        while (t.getState(progressParams())["canStepBack"].as<bool>() && guard++ < 100000) {
            t.stepBack();
        }

        CHECK(readTrav(t) == initial);
        CHECK(!t.getState(progressParams())["canStepBack"].as<bool>());
    }
}

static void testStepBackRestoresEveryIntermediateState() {
    beginTest("stepBack が各時点の状態を正確に復元する");

    TraversalVisualizer t;
    t.load("horizontal", "custom 1 0 0 1\n5 5\n0 1\n1 2\n2 3\n3 4\n0 4\n");
    t.load("setTraversal", traversalCmd("bfs", 0, -1));

    // recs[i] = i 手進めたあとの状態
    std::vector<TravRec> recs;
    recs.push_back(readTrav(t));
    for (int i = 0; i < 100; i++) {
        bool alive = t.step();
        recs.push_back(readTrav(t));
        if (!alive) break;
    }
    CHECK((int)recs.size() > 3);

    for (int i = (int)recs.size() - 1; i >= 0; i--) {
        g_checks++;
        if (!(readTrav(t) == recs[i])) {
            reportFailure(std::to_string(i) + " 手目の状態が復元されていない");
            break;
        }
        if (i > 0) t.stepBack();
    }
}

static void testRunToEndMatchesRepeatedStep() {
    beginTest("runToEnd と step の繰り返しが一致する");

    for (const char* mode : {"bfs", "dfs"}) {
        TraversalVisualizer a, b;
        const char* text = "custom 1 0 0 1\n7 8\n0 1\n0 2\n1 3\n2 4\n3 5\n4 5\n5 6\n1 6\n";
        a.load("horizontal", text);
        b.load("horizontal", text);
        a.load("setTraversal", traversalCmd(mode, 0, 6));
        b.load("setTraversal", traversalCmd(mode, 0, 6));

        runTraversal(a);
        b.runToEnd();

        val sa = a.getState(progressParams()), sb = b.getState(progressParams());
        CHECK(valToVector(sa["visitOrder"]) == valToVector(sb["visitOrder"]));
        CHECK(valToVector(sa["path"]) == valToVector(sb["path"]));
        CHECK_EQ(sa["found"].as<bool>(), sb["found"].as<bool>());
    }
}

static void testTraversalColorsNodesAndEdges() {
    beginTest("探索の進行がノードと辺の色に反映される");

    TraversalVisualizer t;
    t.load("horizontal", "custom 1 0 0 1\n4 3\n0 1\n1 2\n2 3\n");
    t.load("setTraversal", traversalCmd("bfs", 0, 3));

    // 開始直後: 始点だけが処理中、他は未訪問
    val nodes = t.getState(progressParams())["nodes"];
    CHECK_EQ((int)nodes[3].as<float>(), (int)NODE_VISITING);
    CHECK_EQ((int)nodes[7].as<float>(), (int)NODE_DEFAULT);

    t.runToEnd();
    val done = t.getState(progressParams());
    CHECK(done["found"].as<bool>());

    // 経路上の頂点は NODE_PATH、経路の辺は EDGE_PATH
    val n2 = done["nodes"];
    val e2 = done["edges"];
    for (int v : valToVector(done["path"])) {
        CHECK_EQ((int)n2[v * 4 + 3].as<float>(), (int)NODE_PATH);
    }
    int pathEdges = 0;
    for (int i = 0; i < 3; i++) if ((int)e2[i * 4 + 3].as<float>() == (int)EDGE_PATH) pathEdges++;
    CHECK_EQ(pathEdges, 3);
}

static void testTraversalResetsWhenGraphChanges() {
    beginTest("グラフを作り直すと探索がやり直しになる");

    TraversalVisualizer t;
    t.load("horizontal", "custom 1 0 0 1\n5 4\n0 1\n1 2\n2 3\n3 4\n");
    t.load("setTraversal", traversalCmd("bfs", 0, 4));
    t.runToEnd();
    CHECK(t.getState(progressParams())["finished"].as<bool>());

    t.load("horizontal", "custom 1 0 0 1\n3 2\n0 1\n1 2\n");
    val s = t.getState(progressParams());
    CHECK(!s["finished"].as<bool>());
    CHECK_EQ(s["visitOrder"]["length"].as<int>(), 1); // 始点だけ
    CHECK_EQ(s["goalNode"].as<int>(), -1);            // 範囲外になった終点は外れる
}

static void testTraversalHandlesInvalidStart() {
    beginTest("始点が範囲外でも壊れない");

    TraversalVisualizer t;
    t.load("horizontal", "custom 1 0 0 1\n3 2\n0 1\n1 2\n");
    t.load("setTraversal", traversalCmd("bfs", 99, -1));

    // 範囲外の始点は 0 に丸められる
    CHECK_EQ(t.getState(progressParams())["startNode"].as<int>(), 0);
    t.runToEnd();
    CHECK_EQ(t.getState(progressParams())["visitOrder"]["length"].as<int>(), 3);
}

static void testStartEqualsGoal() {
    beginTest("始点と終点が同じなら即座に見つかる");

    TraversalVisualizer t;
    t.load("horizontal", "custom 1 0 0 1\n4 3\n0 1\n1 2\n2 3\n");
    t.load("setTraversal", traversalCmd("bfs", 2, 2));

    val s = t.getState(progressParams());
    CHECK(s["found"].as<bool>());
    CHECK(s["finished"].as<bool>());
    CHECK_EQ(s["path"]["length"].as<int>(), 1);
}


// ==========================================
// ダイクストラ法
// ==========================================

// 参照実装 (単純な O(V^2) のダイクストラ)
static std::vector<float> referenceDijkstra(int v,
                                            const std::vector<std::pair<int,int>>& es,
                                            const std::vector<float>& ws,
                                            bool directed, int src) {
    const float INF_F = std::numeric_limits<float>::infinity();
    std::vector<std::vector<std::pair<int,float>>> adj(v);
    for (size_t i = 0; i < es.size(); i++) {
        adj[es[i].first].push_back({es[i].second, ws[i]});
        if (!directed && es[i].first != es[i].second) {
            adj[es[i].second].push_back({es[i].first, ws[i]});
        }
    }

    std::vector<float> d(v, INF_F);
    std::vector<char> done(v, 0);
    if (src < 0 || src >= v) return d;
    d[src] = 0.0f;

    for (int it = 0; it < v; it++) {
        int u = -1;
        for (int i = 0; i < v; i++) if (!done[i] && d[i] < INF_F && (u < 0 || d[i] < d[u])) u = i;
        if (u < 0) break;
        done[u] = 1;
        for (auto& [to, w] : adj[u]) {
            if (d[u] + w < d[to]) d[to] = d[u] + w;
        }
    }
    return d;
}

static std::vector<float> valToFloats(val arr) {
    std::vector<float> out;
    int n = arr["length"].as<int>();
    for (int i = 0; i < n; i++) out.push_back(arr[i].as<float>());
    return out;
}

static void testDijkstraDistancesMatchReference() {
    beginTest("ダイクストラの距離が参照実装と一致する");

    struct Case {
        const char* text; int v;
        std::vector<std::pair<int,int>> es; std::vector<float> ws; bool directed;
    };
    const Case cases[] = {
        // 遠回りの方が軽い
        {"custom 1 0 0 1\n4 4\n0 1 10\n1 2 10\n2 3 10\n0 3 100\n",
         4, {{0,1},{1,2},{2,3},{0,3}}, {10,10,10,100}, false},
        // 近道の方が軽い
        {"custom 1 0 0 1\n4 4\n0 1 10\n1 2 10\n2 3 10\n0 3 5\n",
         4, {{0,1},{1,2},{2,3},{0,3}}, {10,10,10,5}, false},
        // 有向。向きを守らないと距離が変わる
        {"custom 1 1 0 1\n5 6\n0 1 2\n1 2 3\n0 2 10\n2 3 1\n3 4 4\n0 4 20\n",
         5, {{0,1},{1,2},{0,2},{2,3},{3,4},{0,4}}, {2,3,10,1,4,20}, true},
        // 非連結。届かない頂点は無限大のまま
        {"custom 1 0 0 1\n6 3\n0 1 5\n1 2 5\n4 5 5\n",
         6, {{0,1},{1,2},{4,5}}, {5,5,5}, false},
        // 重み 0 の辺と自己ループ
        {"custom 1 0 0 1\n4 4\n0 0 7\n0 1 0\n1 2 3\n2 3 3\n",
         4, {{0,0},{0,1},{1,2},{2,3}}, {7,0,3,3}, false},
        // 多重辺。軽い方が選ばれる
        {"custom 1 0 0 1\n3 3\n0 1 9\n0 1 2\n1 2 4\n",
         3, {{0,1},{0,1},{1,2}}, {9,2,4}, false},
        // 重みを省略 = すべて 1。幅優先探索と同じ距離になるはず
        {"custom 1 0 0 1\n5 4\n0 1\n1 2\n2 3\n3 4\n",
         5, {{0,1},{1,2},{2,3},{3,4}}, {1,1,1,1}, false},
    };

    for (const auto& c : cases) {
        std::vector<float> expected = referenceDijkstra(c.v, c.es, c.ws, c.directed, 0);

        TraversalVisualizer t;
        t.load("horizontal", c.text);
        t.load("setTraversal", traversalCmd("dijkstra", 0, -1));
        runTraversal(t);

        std::vector<float> got = valToFloats(t.getState(progressParams())["distances"]);

        g_checks++;
        if (got.size() != expected.size()) {
            reportFailure(std::string("距離の個数が違う: ") + c.text);
            continue;
        }
        for (size_t i = 0; i < got.size(); i++) {
            if (got[i] != expected[i]) {
                reportFailure("頂点 " + std::to_string(i) + " の距離が " +
                              std::to_string(got[i]) + " (期待 " +
                              std::to_string(expected[i]) + "): " + c.text);
                break;
            }
        }
    }
}

static void testDijkstraPrefersCheaperDetour() {
    beginTest("ダイクストラは辺の本数ではなく重みで選ぶ");

    // 0->3 は1本で 100、0->1->2->3 は3本で 30。BFS は前者、ダイクストラは後者。
    const char* text = "custom 1 0 0 1\n4 4\n0 1 10\n1 2 10\n2 3 10\n0 3 100\n";

    {
        TraversalVisualizer t;
        t.load("horizontal", text);
        t.load("setTraversal", traversalCmd("dijkstra", 0, 3));
        runTraversal(t);

        val s = t.getState(progressParams());
        CHECK(s["found"].as<bool>());
        CHECK_EQ((int)valToVector(s["path"]).size(), 4);
        CHECK_EQ(s["goalDistance"].as<float>(), 30.0f);
    }
    {
        // 同じグラフを BFS で解くと辺1本の経路になる
        TraversalVisualizer t;
        t.load("horizontal", text);
        t.load("setTraversal", traversalCmd("bfs", 0, 3));
        runTraversal(t);
        CHECK_EQ((int)valToVector(t.getState(progressParams())["path"]).size(), 2);
    }
}

static void testDijkstraReparentsOnRelaxation() {
    beginTest("より軽い経路が見つかったら親を張り替える");

    // 0->2 は直接 100。0->1->2 は 1+1=2。緩和で 2 の親が 0 から 1 に変わる。
    TraversalVisualizer t;
    t.load("horizontal", "custom 1 1 0 1\n3 3\n0 2 100\n0 1 1\n1 2 1\n");
    t.load("setTraversal", traversalCmd("dijkstra", 0, 2));
    runTraversal(t);

    val s = t.getState(progressParams());
    CHECK(s["found"].as<bool>());
    std::vector<int> path = valToVector(s["path"]);
    CHECK_EQ((int)path.size(), 3);
    if (path.size() == 3) {
        CHECK_EQ(path[0], 0);
        CHECK_EQ(path[1], 1);
        CHECK_EQ(path[2], 2);
    }
    CHECK_EQ(s["goalDistance"].as<float>(), 2.0f);
}

static void testDijkstraSettlesInDistanceOrder() {
    beginTest("確定の順序は距離の昇順になる");

    TraversalVisualizer t;
    t.load("horizontal", "custom 1 0 0 1\n5 5\n0 1 7\n0 2 2\n2 1 1\n1 3 3\n3 4 1\n");
    t.load("setTraversal", traversalCmd("dijkstra", 0, -1));
    runTraversal(t);

    val s = t.getState(progressParams());
    std::vector<int> order = valToVector(s["visitOrder"]);
    std::vector<float> d = valToFloats(s["distances"]);

    CHECK_EQ((int)order.size(), 5);
    bool ascending = true;
    for (size_t i = 0; i + 1 < order.size(); i++) {
        if (d[order[i]] > d[order[i + 1]]) ascending = false;
    }
    g_checks++;
    if (!ascending) reportFailure("確定順が距離の昇順になっていない");
}

static void testDijkstraUnreachableGoal() {
    beginTest("届かない終点は無限大のまま見つからない");

    TraversalVisualizer t;
    t.load("horizontal", "custom 1 0 0 1\n5 2\n0 1 3\n3 4 3\n");
    t.load("setTraversal", traversalCmd("dijkstra", 0, 4));
    runTraversal(t);

    val s = t.getState(progressParams());
    CHECK(!s["found"].as<bool>());
    CHECK(s["finished"].as<bool>());
    CHECK(!std::isfinite(valToFloats(s["distances"])[4]));
}

static void testDijkstraStepBackRestoresState() {
    beginTest("ダイクストラでも stepBack が状態を復元する");

    TraversalVisualizer t;
    t.load("horizontal", "custom 1 0 0 1\n6 8\n0 1 4\n0 2 1\n2 1 2\n1 3 5\n2 3 8\n3 4 3\n4 5 1\n2 5 20\n");
    t.load("setTraversal", traversalCmd("dijkstra", 0, 5));

    TravRec initial = readTrav(t);
    CHECK(runTraversal(t) > 0);

    int guard = 0;
    while (t.getState(progressParams())["canStepBack"].as<bool>() && guard++ < 100000) {
        t.stepBack();
    }
    CHECK(readTrav(t) == initial);

    // 距離も初期状態に戻っている
    std::vector<float> d = valToFloats(t.getState(progressParams())["distances"]);
    CHECK_EQ(d[0], 0.0f);
    for (size_t i = 1; i < d.size(); i++) CHECK(!std::isfinite(d[i]));
}

static void testDijkstraReportsNegativeEdges() {
    beginTest("負の重みが混ざっていることを知らせる");

    {
        TraversalVisualizer t;
        t.load("horizontal", "custom 1 0 0 1\n3 2\n0 1 5\n1 2 3\n");
        CHECK(!t.getState(progressParams())["hasNegativeEdge"].as<bool>());
    }
    {
        TraversalVisualizer t;
        t.load("horizontal", "custom 1 0 0 1\n3 2\n0 1 5\n1 2 -3\n");
        CHECK(t.getState(progressParams())["hasNegativeEdge"].as<bool>());
        // 前提を外れていても止まること (無限ループしない)
        t.load("setTraversal", traversalCmd("dijkstra", 0, -1));
        CHECK(runTraversal(t) > 0);
        CHECK(t.getState(progressParams())["finished"].as<bool>());
    }
}

// ==========================================
// 頂点の重み
// ==========================================

static void testNodeWeightsFromText() {
    beginTest("頂点の重みをテキストから読む");

    GraphVisualizer g;
    g.load("horizontal", "custom 1 0 1 1\n4 2\n5 10 15 20\n0 1 3\n1 2 4\n");

    val s = g.getState(progressParams());
    val nodes = s["nodes"];
    CHECK(s["hasNodeWeights"].as<bool>());
    // nodeData = [x, y, weight, colorId] * V
    CHECK_EQ(nodes[2].as<float>(), 5.0f);
    CHECK_EQ(nodes[6].as<float>(), 10.0f);
    CHECK_EQ(nodes[10].as<float>(), 15.0f);
    CHECK_EQ(nodes[14].as<float>(), 20.0f);
    CHECK_EQ(s["edgeCount"].as<int>(), 2);
}

static void testNodeWeightsOmittedIsZero() {
    beginTest("頂点の重みを使わない指定なら 0 のまま");

    GraphVisualizer g;
    g.load("horizontal", "custom 1 0 0 1\n3 2\n0 1\n1 2\n");

    val s = g.getState(progressParams());
    CHECK(!s["hasNodeWeights"].as<bool>());
    val nodes = s["nodes"];
    for (int i = 0; i < 3; i++) CHECK_EQ(nodes[i * 4 + 2].as<float>(), 0.0f);
}

static void testNodeWeightsRoundTripThroughText() {
    beginTest("頂点の重みがテキスト表現に往復する");

    GraphVisualizer g;
    g.load("horizontal", "custom 1 0 1 1\n3 1\n7 8 9\n0 1 2\n");

    val params = val::object();
    params.set("withText", true);
    std::string text = g.getState(params)["graphText"].as<std::string>();

    // "3 1\n7 8 9\n0 1 2\n" の形
    std::istringstream iss(text);
    int v, e;
    iss >> v >> e;
    CHECK_EQ(v, 3);
    CHECK_EQ(e, 1);
    float w0, w1, w2;
    iss >> w0 >> w1 >> w2;
    CHECK_EQ(w0, 7.0f);
    CHECK_EQ(w1, 8.0f);
    CHECK_EQ(w2, 9.0f);
}

static void testDijkstraDoesNotDestroyNodeWeights() {
    beginTest("ダイクストラ後もテキストには元の頂点の重みが残る");

    TraversalVisualizer t;
    t.load("horizontal", "custom 1 0 1 1\n3 2\n11 22 33\n0 1 4\n1 2 4\n");
    t.load("setTraversal", traversalCmd("dijkstra", 0, -1));
    t.runToEnd();

    // 画面表示は暫定距離に置き換わっている
    val nodes = t.getState(progressParams())["nodes"];
    CHECK_EQ(nodes[2].as<float>(), 0.0f);   // 始点の距離
    CHECK_EQ(nodes[6].as<float>(), 4.0f);
    CHECK_EQ(nodes[10].as<float>(), 8.0f);

    // テキストには入力した重みが残る
    val params = val::object();
    params.set("withText", true);
    std::istringstream iss(t.getState(params)["graphText"].as<std::string>());
    int v, e; float w0, w1, w2;
    iss >> v >> e >> w0 >> w1 >> w2;
    CHECK_EQ(w0, 11.0f);
    CHECK_EQ(w1, 22.0f);
    CHECK_EQ(w2, 33.0f);
}


// ==========================================
// 連結なグラフの生成
// ==========================================

// 頂点 0 からの到達可能集合 (向きを尊重する)
static std::vector<char> reachableFromZero(const ParsedGraph& pg, bool directed) {
    std::vector<std::vector<int>> adj(pg.v);
    for (auto& e : pg.edges) {
        adj[e.first].push_back(e.second);
        if (!directed && e.first != e.second) adj[e.second].push_back(e.first);
    }
    std::vector<char> seen(pg.v, 0);
    if (pg.v == 0) return seen;
    std::vector<int> stack{0};
    seen[0] = 1;
    while (!stack.empty()) {
        int u = stack.back(); stack.pop_back();
        for (int w : adj[u]) if (!seen[w]) { seen[w] = 1; stack.push_back(w); }
    }
    return seen;
}

static bool allReachable(const ParsedGraph& pg, bool directed) {
    std::vector<char> seen = reachableFromZero(pg, directed);
    for (int i = 0; i < pg.v; i++) if (!seen[i]) return false;
    return true;
}

static void testConnectedOptionReachesEveryVertex() {
    beginTest("連結指定なら頂点 0 から全頂点へ届く");

    // random V E skip selfLoop sameEdge dir connected
    struct Case { const char* cmd; bool directed; };
    const Case cases[] = {
        {"random 12 15 1 0 0 0 1", false},  // 無向
        {"random 12 15 1 0 0 1 1", true},   // 有向
        {"random 20 25 1 1 1 0 1", false},  // 自己ループ・多重辺あり
        {"random 20 25 1 1 1 1 1", true},
        {"random 30 40 1 0 0 1 1", true},
        {"random 2 1 1 0 0 0 1",  false},   // 最小の連結
    };

    for (const auto& c : cases) {
        for (int trial = 0; trial < 20; trial++) {
            GraphVisualizer g;
            g.load("horizontal", c.cmd);
            ParsedGraph pg = readGraph(g);
            g_checks++;
            if (!allReachable(pg, c.directed)) {
                reportFailure(std::string("届かない頂点がある: ") + c.cmd);
                break;
            }
        }
    }
}

static void testWithoutConnectedOptionItCanBeDisconnected() {
    beginTest("連結指定が無いと非連結になりうる");

    // 辺が少なければ必ずどこかで途切れるはず。何回か試して1回でも出れば良い。
    bool sawDisconnected = false;
    for (int trial = 0; trial < 60 && !sawDisconnected; trial++) {
        GraphVisualizer g;
        g.load("horizontal", "random 20 5 1 0 0 0 0");
        if (!allReachable(readGraph(g), false)) sawDisconnected = true;
    }
    CHECK(sawDisconnected);
}

static void testConnectedRaisesEdgeCountToSpanningTree() {
    beginTest("辺数が足りなければ全域木の分まで引き上げる");

    GraphVisualizer g;
    g.load("horizontal", "random 10 3 1 0 0 0 1"); // 10頂点なのに 3 辺の指定
    ParsedGraph pg = readGraph(g);

    CHECK_EQ(pg.v, 10);
    CHECK_EQ((int)pg.edges.size(), 9); // V-1
    CHECK(allReachable(pg, false));
}

static void testConnectedKeepsRequestedEdgeCount() {
    beginTest("辺数が足りていれば指定どおりの本数になる");

    for (int trial = 0; trial < 10; trial++) {
        GraphVisualizer g;
        g.load("horizontal", "random 8 14 1 0 0 0 1");
        ParsedGraph pg = readGraph(g);
        CHECK_EQ((int)pg.edges.size(), 14);
    }
}

static void testConnectedWithoutMultiEdgeHasNoDuplicates() {
    beginTest("連結指定と多重辺禁止を併用しても辺が重複しない");

    for (int trial = 0; trial < 20; trial++) {
        // 無向
        {
            GraphVisualizer g;
            g.load("horizontal", "random 10 20 1 0 0 0 1");
            std::set<std::pair<int, int>> seen;
            for (auto& e : readGraph(g).edges) {
                auto key = std::make_pair(std::min(e.first, e.second), std::max(e.first, e.second));
                if (!seen.insert(key).second) {
                    g_checks++;
                    reportFailure("無向で重複 " + std::to_string(key.first) + "-" + std::to_string(key.second));
                    return;
                }
            }
        }
        // 有向
        {
            GraphVisualizer g;
            g.load("horizontal", "random 10 25 1 0 0 1 1");
            std::set<std::pair<int, int>> seen;
            for (auto& e : readGraph(g).edges) {
                if (!seen.insert(e).second) {
                    g_checks++;
                    reportFailure("有向で重複 " + std::to_string(e.first) + "->" + std::to_string(e.second));
                    return;
                }
            }
        }
    }
    CHECK(true);
}

static void testConnectedWithTinyGraphs() {
    beginTest("頂点が 0 個 / 1 個でも壊れない");

    {
        GraphVisualizer g;
        g.load("horizontal", "random 0 5 1 0 0 0 1");
        CHECK_EQ(readGraph(g).v, 0);
        CHECK(g.prepare());
    }
    {
        GraphVisualizer g;
        g.load("horizontal", "random 1 5 1 0 0 0 1");
        ParsedGraph pg = readGraph(g);
        CHECK_EQ(pg.v, 1);
        CHECK_EQ((int)pg.edges.size(), 0); // 全域木の辺は不要
        CHECK(g.prepare());
    }
}

static void testConnectedGraphIsFullyTraversed() {
    beginTest("連結指定で生成したグラフは探索が全頂点を訪問する");

    for (const char* mode : {"bfs", "dfs", "dijkstra"}) {
        for (int trial = 0; trial < 10; trial++) {
            TraversalVisualizer t;
            t.load("horizontal", "random 15 20 1 0 0 1 1"); // 有向 + 連結
            t.load("setTraversal", traversalCmd(mode, 0, -1));
            runTraversal(t);

            int visited = t.getState(progressParams())["visitOrder"]["length"].as<int>();
            g_checks++;
            if (visited != 15) {
                reportFailure(std::string(mode) + " が " + std::to_string(visited) +
                              " 頂点しか訪問していない");
                break;
            }
        }
    }
}


// ==========================================
// 円形配置の並び順
// ==========================================

// 実際の座標から、円周上の並び順を復元する
static std::vector<int> ringOrderFromPositions(const ParsedGraph& pg) {
    float cx = 0, cy = 0;
    for (int i = 0; i < pg.v; i++) { cx += pg.xs[i]; cy += pg.ys[i]; }
    cx /= pg.v; cy /= pg.v;

    std::vector<std::pair<float, int>> byAngle;
    for (int i = 0; i < pg.v; i++) {
        byAngle.push_back({std::atan2(pg.ys[i] - cy, pg.xs[i] - cx), i});
    }
    std::sort(byAngle.begin(), byAngle.end());

    std::vector<int> order;
    for (auto& a : byAngle) order.push_back(a.second);
    return order;
}

// 中心からの距離がほぼ一定なら円形配置になっている
static bool looksCircular(const ParsedGraph& pg) {
    if (pg.v < 3) return false;
    float cx = 0, cy = 0;
    for (int i = 0; i < pg.v; i++) { cx += pg.xs[i]; cy += pg.ys[i]; }
    cx /= pg.v; cy /= pg.v;

    float minR = 1e30f, maxR = 0;
    for (int i = 0; i < pg.v; i++) {
        float dx = pg.xs[i] - cx, dy = pg.ys[i] - cy;
        float r = std::sqrt(dx * dx + dy * dy);
        minR = std::min(minR, r);
        maxR = std::max(maxR, r);
    }
    return maxR > 0 && (maxR - minR) / maxR < 0.05f;
}

// 辺の並びから custom コマンドを組み立てる
static std::string customCommand(int v, const std::vector<std::pair<int, int>>& edges) {
    std::ostringstream oss;
    oss << "custom 1 0 0 0\n" << v << " " << edges.size() << "\n";
    for (const auto& e : edges) oss << e.first << " " << e.second << "\n";
    return oss.str();
}

static bool laidOutAsCircle(int v, const std::vector<std::pair<int, int>>& edges) {
    GraphVisualizer g;
    g.load("horizontal", customCommand(v, edges));
    g.prepare();
    return looksCircular(readGraph(g));
}

static std::vector<std::pair<int, int>> starEdges(int n) {
    std::vector<std::pair<int, int>> e;
    for (int i = 1; i < n; i++) e.push_back({0, i});
    return e;
}

static std::vector<std::pair<int, int>> binaryTreeEdges(int n) {
    std::vector<std::pair<int, int>> e;
    for (int i = 1; i < n; i++) e.push_back({(i - 1) / 2, i});
    return e;
}

static std::vector<std::pair<int, int>> gridEdges(int w, int h) {
    std::vector<std::pair<int, int>> e;
    auto id = [&](int r, int c) { return r * w + c; };
    for (int r = 0; r < h; r++) {
        for (int c = 0; c < w; c++) {
            if (c + 1 < w) e.push_back({id(r, c), id(r, c + 1)});
            if (r + 1 < h) e.push_back({id(r, c), id(r + 1, c)});
        }
    }
    return e;
}

static std::vector<std::pair<int, int>> wheelEdges(int n) {
    std::vector<std::pair<int, int>> e;
    for (int i = 1; i < n; i++) e.push_back({0, i});
    for (int i = 1; i < n; i++) e.push_back({i, (i % (n - 1)) + 1});
    return e;
}

static std::vector<std::pair<int, int>> completeEdges(int n) {
    std::vector<std::pair<int, int>> e;
    for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) e.push_back({i, j});
    return e;
}

static void testSparseStructuresKeepForceLayout() {
    beginTest("力学モデルがきれいに描ける形は円形にしない");

    // どれも力学モデルが構造を見せられる形。円形にすると読めなくなる。
    // 特に星は、円周に並べるとハブが縁に来て弦が全部円を横切る。
    CHECK(!laidOutAsCircle(20, starEdges(20)));       // 平均次数 1.9
    CHECK(!laidOutAsCircle(31, binaryTreeEdges(31))); // 1.94
    CHECK(!laidOutAsCircle(25, gridEdges(5, 5)));     // 3.2
    CHECK(!laidOutAsCircle(36, gridEdges(6, 6)));     // 3.33
    CHECK(!laidOutAsCircle(20, wheelEdges(20)));      // 3.8
}

static void testDenseStructuresUseCircularLayout() {
    beginTest("どう置いても辺が交わる密度なら円形にする");

    CHECK(laidOutAsCircle(8, completeEdges(8)));   // 平均次数 7
    CHECK(laidOutAsCircle(12, completeEdges(12))); // 11
}

static void testCircularOnlyForNearCompleteGraphs() {
    beginTest("円形にするのは完全に近いときだけ");

    // 密度 D = 2E / V(V-1)。完全グラフなら 1。
    // 平均次数で切っていたときは、次数8 (密度 0.35) でも円形になっていた。
    auto ring = [](int n, int span) {
        std::vector<std::pair<int, int>> e;
        for (int i = 0; i < n; i++)
            for (int k = 1; k <= span; k++) e.push_back({i, (i + k) % n});
        return e;
    };
    CHECK(!laidOutAsCircle(24, ring(24, 4)));

    // 完全に近ければ、頂点数によらず円形にする
    for (int n : {12, 20}) {
        std::vector<std::pair<int, int>> nearComplete;
        int target = (int)(0.85f * n * (n - 1) / 2);
        for (int i = 0; i < n && (int)nearComplete.size() < target; i++)
            for (int j = i + 1; j < n && (int)nearComplete.size() < target; j++)
                nearComplete.push_back({i, j});
        CHECK(laidOutAsCircle(n, nearComplete));
    }
}

static void testCircularOrderPlacesEveryVertexOnce() {
    beginTest("円周の並び順に漏れも重複もない");

    const int V = 14;
    GraphVisualizer g;
    g.load("horizontal", customCommand(V, completeEdges(V)));
    g.prepare();

    ParsedGraph pg = readGraph(g);
    std::vector<int> order = ringOrderFromPositions(pg);
    std::set<int> unique(order.begin(), order.end());
    CHECK_EQ((int)order.size(), V);
    CHECK_EQ((int)unique.size(), V);

    // 同じ場所に2つ置かれていないか。並び順に重複があると、
    // 2頂点が円周上の同じ枠を取り合って重なる。
    float worst = 1e30f;
    for (int i = 0; i < pg.v; i++) {
        for (int j = i + 1; j < pg.v; j++) {
            float dx = pg.xs[i] - pg.xs[j], dy = pg.ys[i] - pg.ys[j];
            worst = std::min(worst, std::sqrt(dx * dx + dy * dy));
        }
    }
    CHECK(worst > 1.0f);
}

static void testCircularOrderHandlesEdgeCases() {
    beginTest("円形配置になる小さなグラフでも壊れない");

    // 完全グラフ。どう並べても交差は減らせないが、落ちないこと
    const char* cases[] = {
        "complete 3 1 0",
        "complete 5 1 0",
        "complete 12 1 0",
        "custom 1 0 0\n3 3\n0 1\n1 2\n2 0\n",   // 三角形
    };
    for (const char* cmd : cases) {
        GraphVisualizer g;
        g.load("horizontal", cmd);
        CHECK(g.prepare());

        ParsedGraph pg = readGraph(g);
        bool finite = true;
        for (int i = 0; i < pg.v; i++) {
            if (!std::isfinite(pg.xs[i]) || !std::isfinite(pg.ys[i])) finite = false;
        }
        g_checks++;
        if (!finite) reportFailure(std::string("座標が有限でない: ") + cmd);
    }
}

// ==========================================
// 重み付き / 重み無しの区別
// ==========================================

static std::string graphTextOf(GraphVisualizer& g) {
    val params = val::object();
    params.set("withText", true);
    return g.getState(params)["graphText"].as<std::string>();
}

// 辺の行の列数を数える (1行目の "V E" は除く)
static std::vector<int> edgeLineColumns(const std::string& text) {
    std::istringstream iss(text);
    std::string line;
    std::getline(iss, line); // "V E"

    std::vector<int> columns;
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        std::istringstream ls(line);
        int count = 0;
        float x;
        while (ls >> x) count++;
        columns.push_back(count);
    }
    return columns;
}

static void testUnweightedGraphHasNoWeightColumn() {
    beginTest("重み無しならテキストは2列で、書き戻しても2列のまま");

    GraphVisualizer g;
    g.load("horizontal", "random 8 12 1 0 0 0 1 0"); // 末尾 0 = 重み無し
    CHECK(!g.getState(val::object())["weighted"].as<bool>());

    std::string text = graphTextOf(g);
    for (int c : edgeLineColumns(text)) CHECK_EQ(c, 2);

    // 書き戻しても2列のまま。ここが崩れると「重み無しにしたのに重みが付いて返る」
    std::ostringstream cmd;
    cmd << "custom 1 0 0 0\n" << text;
    GraphVisualizer again;
    again.load("horizontal", cmd.str());
    for (int c : edgeLineColumns(graphTextOf(again))) CHECK_EQ(c, 2);

    // 重み付きなら3列
    GraphVisualizer w;
    w.load("horizontal", "random 8 12 1 0 0 0 1 1");
    CHECK(w.getState(val::object())["weighted"].as<bool>());
    for (int c : edgeLineColumns(graphTextOf(w))) CHECK_EQ(c, 3);
}

static void testCustomHeaderDoesNotEatTheBody() {
    beginTest("custom の引数を省略しても本文を食わない");

    // ヘッダを行またぎで読むと、省略した引数の代わりに本文の数値を吸い込んで
    // グラフが丸ごと別物になる。1行目だけから読むこと。
    GraphVisualizer g;
    g.load("horizontal", "custom 1\n4 3\n0 1\n1 2\n2 3\n"); // dir 以降を省略

    ParsedGraph pg = readGraph(g);
    CHECK_EQ(pg.v, 4);
    CHECK_EQ((int)pg.edges.size(), 3);
}

static void testNodeWeightLineDoesNotEatAnEdge() {
    beginTest("頂点の重み行が無いテキストでも辺が消えない");

    // 「頂点の重みを入力する」を指定しているのに重み行が無い。
    // 無条件に1行消費すると、最初の辺が食われて辺が1本消える。
    GraphVisualizer g;
    g.load("horizontal", "custom 1 0 1 0\n4 3\n0 1\n1 2\n2 3\n");

    ParsedGraph pg = readGraph(g);
    CHECK_EQ(pg.v, 4);
    CHECK_EQ((int)pg.edges.size(), 3);
    if (pg.edges.size() == 3) {
        CHECK(pg.edges[0] == std::make_pair(0, 1));
        CHECK(pg.edges[1] == std::make_pair(1, 2));
        CHECK(pg.edges[2] == std::make_pair(2, 3));
    }

    // 重み行があるときは今までどおり読める (行数の判定で取り違えないこと)
    GraphVisualizer h;
    h.load("horizontal", "custom 1 0 1 0\n4 3\n5 6 7 8\n0 1\n1 2\n2 3\n");
    ParsedGraph ph = readGraph(h);
    CHECK_EQ((int)ph.edges.size(), 3);
    val nodes = h.getState(val::object())["nodes"];
    CHECK_EQ(nodes[2].as<float>(), 5.0f);
}

static void testDijkstraOnUnweightedCountsEdges() {
    beginTest("重み無しのダイクストラの距離は辺の本数と一致する");

    // 0-1-2-3-4 の道。重み無しなら距離はそのままホップ数になる。
    TraversalVisualizer t;
    t.load("horizontal", "custom 1 0 0 0\n5 4\n0 1\n1 2\n2 3\n3 4\n");
    t.load("setTraversal", traversalCmd("dijkstra", 0, -1));
    runTraversal(t);

    val s = t.getState(progressParams());
    std::vector<float> d = valToFloats(s["distances"]);
    CHECK_EQ((int)d.size(), 5);
    for (int i = 0; i < (int)d.size(); i++) CHECK_EQ(d[i], (float)i);
}


// ==========================================
// DFA (決定性有限オートマトン)
// ==========================================

struct DfaEdge { int from, to; char sym; };

static std::vector<DfaEdge> readDfaEdges(AutomatonVisualizer& a, int* vOut = nullptr) {
    val params = val::object();
    params.set("withText", true);
    std::string text = a.getState(params)["graphText"].as<std::string>();

    std::istringstream iss(text);
    int v = 0, e = 0;
    iss >> v >> e;
    if (vOut) *vOut = v;

    std::vector<DfaEdge> out;
    int from, to; char sym;
    while ((int)out.size() < e && (iss >> from >> to >> sym)) out.push_back({from, to, sym});
    return out;
}

// 「a が偶数個なら受理」の DFA。状態 0 が偶数、状態 1 が奇数。
static const char* EVEN_A_BODY =
    "2 4\n0 1 a\n0 0 b\n1 0 a\n1 1 b\n";

static void loadDfa(AutomatonVisualizer& a, const std::string& body,
                    const std::string& start, const std::string& accepting) {
    a.load("horizontal", "custom 1 1 0 0\n" + body);
    a.load("setStartNode", start);
    a.load("setAccepting", accepting);
}

static bool acceptsDfa(AutomatonVisualizer& a, const std::string& input) {
    a.load("setInput", input);
    a.runToEnd();
    return a.getState(val::object())["accepted"].as<bool>();
}

static void testDfaAcceptsAndRejects() {
    beginTest("既知の DFA が受理すべき文字列だけを受理する");

    AutomatonVisualizer a;
    loadDfa(a, EVEN_A_BODY, "0", "0");

    // a が偶数個 (0 個も偶数)
    CHECK(acceptsDfa(a, ""));
    CHECK(acceptsDfa(a, "b"));
    CHECK(acceptsDfa(a, "aa"));
    CHECK(acceptsDfa(a, "aba"));
    CHECK(acceptsDfa(a, "bbaabb"));

    // a が奇数個
    CHECK(!acceptsDfa(a, "a"));
    CHECK(!acceptsDfa(a, "ab"));
    CHECK(!acceptsDfa(a, "aaa"));
    CHECK(!acceptsDfa(a, "bab"));
}

static void testDfaStepMatchesRunToEnd() {
    beginTest("step を繰り返した結果と runToEnd の結果が一致する");

    const char* inputs[] = {"", "a", "abab", "bbaabb", "aaaaa"};
    for (const char* in : inputs) {
        AutomatonVisualizer stepwise;
        loadDfa(stepwise, EVEN_A_BODY, "0", "0");
        stepwise.load("setInput", in);
        while (stepwise.step()) {}

        AutomatonVisualizer atOnce;
        loadDfa(atOnce, EVEN_A_BODY, "0", "0");
        atOnce.load("setInput", in);
        atOnce.runToEnd();

        val s = stepwise.getState(val::object());
        val o = atOnce.getState(val::object());
        CHECK_EQ(s["accepted"].as<bool>(), o["accepted"].as<bool>());
        CHECK_EQ(s["currentState"].as<int>(), o["currentState"].as<int>());
        CHECK_EQ(s["inputPos"].as<int>(), o["inputPos"].as<int>());
        CHECK_EQ(s["finished"].as<bool>(), o["finished"].as<bool>());
    }
}

static void testDfaStepBackReturnsToPreviousState() {
    beginTest("stepBack で1手前の状態に戻る");

    AutomatonVisualizer a;
    loadDfa(a, EVEN_A_BODY, "0", "0");
    a.load("setInput", "aba");

    // 1手進めるたびに (現在状態, 消費位置) を控えておく
    std::vector<std::pair<int, int>> seen;
    val s = a.getState(val::object());
    seen.push_back({s["currentState"].as<int>(), s["inputPos"].as<int>()});
    CHECK(!s["canStepBack"].as<bool>());

    while (a.step()) {
        val cur = a.getState(val::object());
        seen.push_back({cur["currentState"].as<int>(), cur["inputPos"].as<int>()});
    }

    // 「読み切って判定する」のも1手なので、最初の stepBack は判定の取り消しになる。
    // 受理 / 拒否の表示から1手戻れる方が、見ていて自然。
    a.stepBack();
    val undone = a.getState(val::object());
    CHECK(!undone["finished"].as<bool>());
    CHECK_EQ(undone["currentState"].as<int>(), seen.back().first);
    CHECK_EQ(undone["inputPos"].as<int>(), seen.back().second);

    // そこからは、控えた並びをそのまま逆順に辿り直す
    for (int i = (int)seen.size() - 1; i >= 1; i--) {
        a.stepBack();
        val cur = a.getState(val::object());
        CHECK_EQ(cur["currentState"].as<int>(), seen[i - 1].first);
        CHECK_EQ(cur["inputPos"].as<int>(), seen[i - 1].second);
    }
    CHECK(!a.getState(val::object())["canStepBack"].as<bool>());
}

static void testDfaStopsWhenTransitionIsMissing() {
    beginTest("遷移が無いところで stuck になって止まる");

    // 状態 1 から a の遷移が無い
    AutomatonVisualizer a;
    loadDfa(a, "2 3\n0 1 a\n0 0 b\n1 1 b\n", "0", "1");

    a.load("setInput", "aa");
    a.runToEnd();
    val s = a.getState(val::object());
    CHECK(s["stuck"].as<bool>());
    CHECK(s["finished"].as<bool>());
    CHECK(!s["accepted"].as<bool>());
    CHECK_EQ(s["inputPos"].as<int>(), 1); // a を1文字消費したところで止まる

    // アルファベットに無い記号でも同じ
    a.load("setInput", "c");
    a.runToEnd();
    CHECK(a.getState(val::object())["stuck"].as<bool>());

    // 遷移がある範囲なら止まらない
    a.load("setInput", "abb");
    a.runToEnd();
    val ok = a.getState(val::object());
    CHECK(!ok["stuck"].as<bool>());
    CHECK(ok["accepted"].as<bool>());
}

static void testDfaDetectsNondeterminism() {
    beginTest("同じ状態・同じ記号の遷移が2本あると非決定性を知らせる");

    AutomatonVisualizer nd;
    loadDfa(nd, "2 3\n0 1 a\n0 0 a\n1 1 b\n", "0", "1");
    CHECK(nd.getState(val::object())["hasNondeterminism"].as<bool>());

    AutomatonVisualizer d;
    loadDfa(d, EVEN_A_BODY, "0", "0");
    CHECK(!d.getState(val::object())["hasNondeterminism"].as<bool>());
}

static void testDfaTextRoundTrip() {
    beginTest("遷移規則のテキストが記号ごと書き戻せる");

    AutomatonVisualizer a;
    loadDfa(a, EVEN_A_BODY, "0", "0");

    int v = 0;
    std::vector<DfaEdge> edges = readDfaEdges(a, &v);
    CHECK_EQ(v, 2);
    CHECK_EQ((int)edges.size(), 4);
    if (edges.size() == 4) {
        CHECK_EQ(edges[0].sym, 'a');
        CHECK_EQ(edges[1].sym, 'b');
        CHECK_EQ(edges[0].to, 1);
        CHECK_EQ(edges[1].to, 0);
    }

    // 書き戻したテキストをもう一度読ませても同じ言語を受理する
    val params = val::object();
    params.set("withText", true);
    std::string text = a.getState(params)["graphText"].as<std::string>();

    AutomatonVisualizer again;
    loadDfa(again, text, "0", "0");
    CHECK(acceptsDfa(again, "abab"));  // a が2個
    CHECK(!acceptsDfa(again, "ab"));   // a が1個
}

static void testGeneratedDfaIsDeterministicAndPartial() {
    beginTest("ランダム生成した DFA は決定性で、遷移に空きがある");

    // "aab" は重複した指定。記号としては ab の2つにまとまる。
    // まとまっていないと、同じ状態から a の遷移が2本出て決定性が崩れる。
    const char* alphabets[] = { "abc", "aab" };
    for (int trial = 0; trial < 20; trial++) {
        AutomatonVisualizer a;
        a.load("genRandom", std::string("8 ") + alphabets[trial % 2]);

        int v = 0;
        std::vector<DfaEdge> edges = readDfaEdges(a, &v);
        CHECK_EQ(v, 8);

        // (状態, 記号) の組は高々1回。決定性は構成から保証される
        std::set<std::pair<int, char>> seen;
        for (const DfaEdge& e : edges) {
            CHECK(seen.insert({e.from, e.sym}).second);
            CHECK(e.to >= 0 && e.to < v);
        }

        // 完全ではない。状態数 x 記号数だけ辺を張ると図がすぐ絡まるうえ、
        // 「遷移が無くて拒否される」様子が一度も見られなくなる
        CHECK((int)edges.size() < 8 * 3);

        // 初期状態からすべての状態へ届く。届かない状態は入力をどう与えても
        // 光らないので、置いても読めない
        std::vector<std::vector<int>> next(v);
        for (const DfaEdge& e : edges) next[e.from].push_back(e.to);
        std::vector<char> visited(v, 0);
        std::vector<int> stack{0};
        visited[0] = 1;
        while (!stack.empty()) {
            int u = stack.back(); stack.pop_back();
            for (int w : next[u]) if (!visited[w]) { visited[w] = 1; stack.push_back(w); }
        }
        CHECK_EQ((int)std::count(visited.begin(), visited.end(), (char)1), v);
    }
}

static void testUndefinedTransitionIsARejection() {
    beginTest("遷移が無い記号を読んだら、その時点で拒否になる");

    // 状態 1 から a の遷移が無い。1 は受理状態だが、a を読んだ時点で拒否になる。
    AutomatonVisualizer a;
    loadDfa(a, "2 3\n0 1 a\n0 0 b\n1 1 b\n", "0", "1");

    a.load("setInput", "aa");
    a.runToEnd();
    val s = a.getState(val::object());
    CHECK(s["stuck"].as<bool>());
    CHECK(s["finished"].as<bool>());
    CHECK_EQ(s["currentState"].as<int>(), 1);
    CHECK(!s["accepted"].as<bool>());
}

static GraphData makeTree(int n, const std::vector<std::pair<int, int>>& edges) {
    GraphData g(n, (int)edges.size());
    for (int i = 0; i < n; i++) g.setNode(i, 0.0f, 0.0f, (float)i, 0);
    for (const auto& e : edges) g.addEdge((float)e.first, (float)e.second, 0.0f, 0);
    return g;
}

static void placeTree(GraphData& g, TreeLayout& layout) {
    std::vector<std::vector<int>> adj(g.nodeCount());
    layout.init(&g, adj);
    layout.finish(&g);
}

static float nodeX(const GraphData& g, int i) { return g.nodeData[i * GraphData::NODE_STRIDE]; }
static float nodeY(const GraphData& g, int i) { return g.nodeData[i * GraphData::NODE_STRIDE + 1]; }

static void testTreeDepthBecomesY() {
    beginTest("深さがそのまま y になる");

    //   0
    //  / \
    // 1   2
    //     |
    //     3
    GraphData g = makeTree(4, {{0, 1}, {0, 2}, {2, 3}});
    TreeLayout layout;
    placeTree(g, layout);

    CHECK_EQ(nodeY(g, 0), 0.0f);
    CHECK_EQ(nodeY(g, 1), TreeLayout::LEVEL_GAP);
    CHECK_EQ(nodeY(g, 2), TreeLayout::LEVEL_GAP);
    CHECK_EQ(nodeY(g, 3), TreeLayout::LEVEL_GAP * 2.0f);
}

static void testTreeParentIsCenteredOverChildren() {
    beginTest("親が子たちの中央に来る");

    GraphData g = makeTree(3, {{0, 1}, {0, 2}});
    TreeLayout layout;
    placeTree(g, layout);
    CHECK_NEAR(nodeX(g, 0), (nodeX(g, 1) + nodeX(g, 2)) / 2.0f, 0.01f);

    // 深い部分木を片側に持つ形でも、端の子2つの中央に来る
    //       0
    //     /   \
    //    1     2
    //   / \
    //  3   4
    GraphData h = makeTree(5, {{0, 1}, {0, 2}, {1, 3}, {1, 4}});
    TreeLayout layout2;
    placeTree(h, layout2);
    CHECK_NEAR(nodeX(h, 0), (nodeX(h, 1) + nodeX(h, 2)) / 2.0f, 0.01f);
    CHECK_NEAR(nodeX(h, 1), (nodeX(h, 3) + nodeX(h, 4)) / 2.0f, 0.01f);
}

static void testTreeNodesDoNotOverlap() {
    beginTest("同じ深さの節点が最小の間隔より近づかない");

    // 両側に部分木がぶら下がる形。輪郭を見ずに置くと、
    // 1 の右の子と 2 の左の子がぶつかる。
    //        0
    //     /     \
    //    1       2
    //   / \    / \
    //  3   4   5   6
    GraphData g = makeTree(7, {{0, 1}, {0, 2}, {1, 3}, {1, 4}, {2, 5}, {2, 6}});
    TreeLayout layout;
    placeTree(g, layout);

    // 深さごとにまとめて、隣り合う節点の間隔を見る
    std::map<float, std::vector<float>> rows;
    for (int i = 0; i < g.nodeCount(); i++) rows[nodeY(g, i)].push_back(nodeX(g, i));
    for (auto& row : rows) {
        std::sort(row.second.begin(), row.second.end());
        for (std::size_t i = 1; i < row.second.size(); i++) {
            CHECK(row.second[i] - row.second[i - 1] >= TreeLayout::SIBLING_GAP - 0.01f);
        }
    }
}

static void testForestIsPlacedSideBySide() {
    beginTest("森は左右に分かれて重ならない");

    // 木が2つ (0-1-2 と 3-4)
    GraphData g = makeTree(5, {{0, 1}, {0, 2}, {3, 4}});
    TreeLayout layout;
    placeTree(g, layout);

    // 片方の木の右端より、もう片方の木の左端が右にある
    float leftMax = std::max({nodeX(g, 0), nodeX(g, 1), nodeX(g, 2)});
    float rightMin = std::min(nodeX(g, 3), nodeX(g, 4));
    CHECK(rightMin > leftMax);
}

static void testTreeLayoutHandlesEdgeCases() {
    beginTest("節点が0個・1個・閉路混じりでも落ちない");

    GraphData empty(0, 0);
    TreeLayout l0;
    placeTree(empty, l0);
    CHECK(l0.isStable());

    GraphData single = makeTree(1, {});
    TreeLayout l1;
    placeTree(single, l1);
    CHECK_EQ(nodeY(single, 0), 0.0f);

    // 閉路だけの入力。根が1つも無いので、無限再帰せずに置き切れること
    GraphData cyclic = makeTree(3, {{0, 1}, {1, 2}, {2, 0}});
    TreeLayout l2;
    placeTree(cyclic, l2);
    CHECK(l2.isStable());
    for (int i = 0; i < 3; i++) CHECK(std::isfinite(nodeX(cyclic, i)));
}

static void testTreeLayoutEasesToTarget() {
    beginTest("update を繰り返すと finish と同じ座標に落ち着く");

    std::vector<std::pair<int, int>> edges = {{0, 1}, {0, 2}, {1, 3}, {1, 4}};

    GraphData atOnce = makeTree(5, edges);
    TreeLayout la;
    placeTree(atOnce, la);

    GraphData eased = makeTree(5, edges);
    TreeLayout le;
    std::vector<std::vector<int>> adj(eased.nodeCount());
    le.init(&eased, adj);
    int frames = 0;
    while (!le.update(&eased) && frames < 500) frames++;

    CHECK(le.isStable());
    CHECK(frames < 500); // 収束せずに打ち切られていない
    for (int i = 0; i < 5; i++) {
        CHECK_NEAR(nodeX(eased, i), nodeX(atOnce, i), 0.01f);
        CHECK_NEAR(nodeY(eased, i), nodeY(atOnce, i), 0.01f);
    }
}


// ==========================================
// 二分探索木の構築
// ==========================================

struct ParsedTree {
    int root = -1;
    std::vector<float> values;
    std::vector<std::vector<int>> children; // 値の小さい順
};

static ParsedTree readTree(BstVisualizer& b) {
    val s = b.getState(val::object());
    ParsedTree t;
    t.root = s["startNodeIndex"].as<int>();

    val nodes = s["nodes"];
    int n = s["nodeCount"].as<int>();
    for (int i = 0; i < n; i++) {
        t.values.push_back(nodes[i * GraphData::NODE_STRIDE + 2].as<float>());
    }
    t.children.assign(n, {});

    val edges = s["edges"];
    int m = s["edgeCount"].as<int>();
    for (int i = 0; i < m; i++) {
        int from = (int)edges[i * GraphData::EDGE_STRIDE].as<float>();
        int to   = (int)edges[i * GraphData::EDGE_STRIDE + 1].as<float>();
        if (from >= 0 && from < n && to >= 0 && to < n) t.children[from].push_back(to);
    }
    for (int i = 0; i < n; i++) {
        std::sort(t.children[i].begin(), t.children[i].end(),
                  [&](int a, int c) { return t.values[a] < t.values[c]; });
    }
    return t;
}

// 中順走査。二分探索木なら昇順に並ぶ。
static void inorder(const ParsedTree& t, int u, std::vector<float>& out, int depth = 0) {
    if (u < 0 || u >= (int)t.values.size() || depth > 100) return;
    const std::vector<int>& kids = t.children[u];
    // 値の小さい子が左。子が1つのときは値の大小で左右が決まる
    for (int c : kids) if (t.values[c] < t.values[u]) inorder(t, c, out, depth + 1);
    out.push_back(t.values[u]);
    for (int c : kids) if (t.values[c] > t.values[u]) inorder(t, c, out, depth + 1);
}

static void buildBst(BstVisualizer& b, const std::string& values) {
    b.load("setValues", values);
    b.runToEnd();
}

static void testBstInorderIsSorted() {
    beginTest("挿入後の木を中順に辿ると昇順になる");

    const char* inputs[] = {
        "50 30 70 20 40 60 80",
        "1 2 3 4 5",          // 右に伸び続ける
        "5 4 3 2 1",          // 左に伸び続ける
        "42",
    };
    for (const char* in : inputs) {
        BstVisualizer b;
        buildBst(b, in);

        ParsedTree t = readTree(b);
        std::vector<float> order;
        inorder(t, t.root, order);

        // 入力の値がすべて現れ、しかも昇順
        std::vector<float> expected;
        std::istringstream iss(in);
        float v;
        while (iss >> v) expected.push_back(v);
        std::sort(expected.begin(), expected.end());

        CHECK_EQ((int)order.size(), (int)expected.size());
        CHECK(order == expected);
    }
}

static void testBstDrawsSmallerValuesOnTheLeft() {
    beginTest("小さい値が左に描かれる");

    // 挿入の順と左右の並びが食い違う形。辺の追加順のまま並べると
    // 70 が 30 の左に来てしまう。
    BstVisualizer b;
    buildBst(b, "50 70 30");

    val s = b.getState(val::object());
    CHECK_EQ(s["nodeCount"].as<int>(), 3);

    val nodes = s["nodes"];
    // 節点の値から x 座標を引く
    std::map<int, float> xOf;
    for (int i = 0; i < s["nodeCount"].as<int>(); i++) {
        int v = (int)nodes[i * GraphData::NODE_STRIDE + 2].as<float>();
        xOf[v] = nodes[i * GraphData::NODE_STRIDE].as<float>();
    }

    // レイアウトを確定させてから比べる
    for (int i = 0; i < 200; i++) b.prepare();
    val after = b.getState(val::object());
    val an = after["nodes"];
    for (int i = 0; i < after["nodeCount"].as<int>(); i++) {
        int v = (int)an[i * GraphData::NODE_STRIDE + 2].as<float>();
        xOf[v] = an[i * GraphData::NODE_STRIDE].as<float>();
    }

    CHECK(xOf[30] < xOf[50]);
    CHECK(xOf[50] < xOf[70]);
}

static void testBstIgnoresDuplicates() {
    beginTest("同じ値を2回入れても節点が増えない");

    BstVisualizer b;
    buildBst(b, "5 3 8 3 5 8");
    val s = b.getState(val::object());
    CHECK_EQ(s["nodeCount"].as<int>(), 3);
    CHECK_EQ(s["edgeCount"].as<int>(), 2);
}

static void testBstStepMatchesRunToEnd() {
    beginTest("step を繰り返した結果と runToEnd の結果が一致する");

    const char* inputs[] = {"50 30 70 20 40", "1 2 3", "9 9 9", ""};
    for (const char* in : inputs) {
        BstVisualizer stepwise;
        stepwise.load("setValues", in);
        int guard = 0;
        while (stepwise.step() && guard++ < 1000) {}

        BstVisualizer atOnce;
        buildBst(atOnce, in);

        val a = stepwise.getState(val::object());
        val o = atOnce.getState(val::object());
        CHECK_EQ(a["nodeCount"].as<int>(), o["nodeCount"].as<int>());
        CHECK_EQ(a["edgeCount"].as<int>(), o["edgeCount"].as<int>());
        CHECK_EQ(a["finished"].as<bool>(), o["finished"].as<bool>());

        std::vector<float> sa, so;
        ParsedTree ta = readTree(stepwise), to = readTree(atOnce);
        inorder(ta, ta.root, sa);
        inorder(to, to.root, so);
        CHECK(sa == so);
    }
}

static void testBstStepBackReturnsToPreviousState() {
    beginTest("stepBack で1手前の状態に戻る");

    BstVisualizer b;
    b.load("setValues", "50 30 70 20");

    // 1手ずつ進めて、そのつどの (節点数, 比較中, 挿入位置) を控える
    std::vector<std::array<int, 3>> seen;
    auto snapshot = [&]() {
        val s = b.getState(val::object());
        return std::array<int, 3>{ s["nodeCount"].as<int>(),
                                   s["cursor"].as<int>(),
                                   s["pending"].as<int>() };
    };
    seen.push_back(snapshot());
    CHECK(!b.getState(val::object())["canStepBack"].as<bool>());

    int guard = 0;
    while (b.step() && guard++ < 1000) seen.push_back(snapshot());

    // 逆順に戻ると、控えた並びをそのまま辿り直す
    for (int i = (int)seen.size() - 1; i >= 1; i--) {
        b.stepBack();
        CHECK(snapshot() == seen[i - 1]);
    }
    CHECK(!b.getState(val::object())["canStepBack"].as<bool>());
}

static void testBstHandlesEmptyInput() {
    beginTest("値が1つも無くても落ちない");

    BstVisualizer b;
    buildBst(b, "");
    val s = b.getState(val::object());
    CHECK_EQ(s["nodeCount"].as<int>(), 0);
    CHECK(s["finished"].as<bool>());
    CHECK(!s["canStepBack"].as<bool>());

    b.stepBack(); // 戻せないときに何も壊さない
    CHECK_EQ(b.getState(val::object())["nodeCount"].as<int>(), 0);
}

static void testGeneratedValuesHaveNoDuplicates() {
    beginTest("ランダム生成した値に重複が無い");

    BstVisualizer b;
    b.load("genRandom", "20");
    b.runToEnd();

    val s = b.getState(val::object());
    val vals = s["values"];
    int count = vals["length"].as<int>();
    CHECK_EQ(count, 20);

    std::set<int> unique;
    for (int i = 0; i < count; i++) unique.insert(vals[i].as<int>());
    CHECK_EQ((int)unique.size(), 20);

    // 重複が無いので、節点の数は値の数と一致する
    CHECK_EQ(s["nodeCount"].as<int>(), 20);
}


// ==========================================
// ヒープの構築
// ==========================================

struct ParsedHeap {
    int n = 0;
    std::vector<float> values;                 // 節点の添字ごとの値
    std::vector<std::pair<int, int>> edges;    // (親, 子)
    std::vector<float> xs, ys;
};

static ParsedHeap readHeap(HeapVisualizer& h) {
    val s = h.getState(val::object());
    ParsedHeap ph;
    ph.n = s["nodeCount"].as<int>();

    val nodes = s["nodes"];
    for (int i = 0; i < ph.n; i++) {
        ph.values.push_back(nodes[i * GraphData::NODE_STRIDE + 2].as<float>());
        ph.xs.push_back(nodes[i * GraphData::NODE_STRIDE].as<float>());
        ph.ys.push_back(nodes[i * GraphData::NODE_STRIDE + 1].as<float>());
    }

    val edges = s["edges"];
    int m = s["edgeCount"].as<int>();
    for (int i = 0; i < m; i++) {
        ph.edges.push_back({ (int)edges[i * GraphData::EDGE_STRIDE].as<float>(),
                             (int)edges[i * GraphData::EDGE_STRIDE + 1].as<float>() });
    }
    return ph;
}

static ParsedHeap buildHeap(const std::string& values, bool maxHeap = true) {
    HeapVisualizer h;
    if (!maxHeap) h.load("setMaxHeap", "0");
    h.load("setValues", values);
    h.runToEnd();
    return readHeap(h);
}

static void testHeapConditionHolds() {
    beginTest("挿入し終えた木がヒープ条件を満たす");

    const char* inputs[] = { "50 30 70 20 40 60 80", "1 2 3 4 5 6 7 8 9", "9 8 7 6 5", "42" };
    for (const char* in : inputs) {
        ParsedHeap ph = buildHeap(in);
        for (int i = 1; i < ph.n; i++) {
            g_checks++;
            if (ph.values[i] > ph.values[(i - 1) / 2]) {
                reportFailure(std::string(in) + ": 節点 " + std::to_string(i) +
                              " (" + std::to_string((int)ph.values[i]) + ") が親 (" +
                              std::to_string((int)ph.values[(i - 1) / 2]) + ") より大きい");
            }
        }
        // 値の集合は入れたものと一致する
        std::vector<float> expected;
        std::istringstream iss(in);
        float v;
        while (iss >> v) expected.push_back(v);
        std::vector<float> got = ph.values;
        std::sort(expected.begin(), expected.end());
        std::sort(got.begin(), got.end());
        CHECK(got == expected);
    }
}

static void testMinHeapReversesTheOrder() {
    beginTest("最小ヒープに切り替えると親が子以下になる");

    ParsedHeap ph = buildHeap("50 30 70 20 40 60 80", false);
    CHECK_EQ(ph.n, 7);
    for (int i = 1; i < ph.n; i++) {
        g_checks++;
        if (ph.values[i] < ph.values[(i - 1) / 2]) {
            reportFailure("節点 " + std::to_string(i) + " が親より小さい");
        }
    }
    if (ph.n > 0) CHECK_EQ(ph.values[0], 20.0f); // 根は最小値
}

static void testHeapShapeIsCompleteBinaryTree() {
    beginTest("形が完全二分木になっている");

    ParsedHeap ph = buildHeap("50 30 70 20 40 60 80 10 90");
    CHECK_EQ(ph.n, 9);
    CHECK_EQ((int)ph.edges.size(), 8); // 根以外はちょうど1本の親を持つ

    // 辺は必ず ((i-1)/2, i) の形
    std::set<int> hasParent;
    for (const auto& e : ph.edges) {
        CHECK_EQ(e.first, (e.second - 1) / 2);
        CHECK(hasParent.insert(e.second).second); // 親は1つだけ
    }
    for (int i = 1; i < ph.n; i++) CHECK(hasParent.count(i) == 1);
}

static void testSwapMovesNodesInsteadOfJumping() {
    beginTest("値の入れ替えは座標も入れ替えて動きで見せる");

    // 10 を置く / 根なので止まる / 20 を置く、まで進めてから配置を落ち着かせる
    HeapVisualizer h;
    h.load("setValues", "10 20");
    for (int i = 0; i < 3; i++) h.step();
    for (int i = 0; i < 500; i++) h.prepare();

    ParsedHeap before = readHeap(h);
    CHECK_EQ(before.n, 2);
    CHECK(before.ys[0] != before.ys[1]); // 根と子で高さが違う

    h.step(); // ここで 20 が親と入れ替わる
    ParsedHeap after = readHeap(h);

    // 値は入れ替わっている
    CHECK_EQ(after.values[0], 20.0f);
    CHECK_EQ(after.values[1], 10.0f);

    // 今いる座標も入れ替わっている。ここから目標へ戻る間に
    // 「値が上がっていった」ように見える。
    CHECK_NEAR(after.ys[0], before.ys[1], 0.01f);
    CHECK_NEAR(after.ys[1], before.ys[0], 0.01f);

    // 落ち着けば元の高さに戻る (目標の座標は動かしていない)
    for (int i = 0; i < 500; i++) h.prepare();
    ParsedHeap settled = readHeap(h);
    CHECK_NEAR(settled.ys[0], before.ys[0], 0.01f);
    CHECK_NEAR(settled.ys[1], before.ys[1], 0.01f);
}

static void testHeapStepMatchesRunToEnd() {
    beginTest("step を繰り返した結果と runToEnd の結果が一致する");

    const char* inputs[] = { "50 30 70 20 40", "1 2 3", "" };
    for (const char* in : inputs) {
        HeapVisualizer stepwise;
        stepwise.load("setValues", in);
        int guard = 0;
        while (stepwise.step() && guard++ < 2000) {}

        HeapVisualizer atOnce;
        atOnce.load("setValues", in);
        atOnce.runToEnd();

        ParsedHeap a = readHeap(stepwise), b = readHeap(atOnce);
        CHECK_EQ(a.n, b.n);
        CHECK(a.values == b.values);
        CHECK(a.edges == b.edges);
    }
}

static void testHeapStepBackReturnsToPreviousState() {
    beginTest("stepBack で1手前の状態に戻る");

    HeapVisualizer h;
    h.load("setValues", "50 30 70 20");

    auto snapshot = [&]() {
        val s = h.getState(val::object());
        ParsedHeap ph = readHeap(h);
        return std::make_pair(std::make_pair(s["nodeCount"].as<int>(), s["cursor"].as<int>()),
                              ph.values);
    };

    std::vector<decltype(snapshot())> seen;
    seen.push_back(snapshot());
    CHECK(!h.getState(val::object())["canStepBack"].as<bool>());

    int guard = 0;
    while (h.step() && guard++ < 2000) seen.push_back(snapshot());

    for (int i = (int)seen.size() - 1; i >= 1; i--) {
        h.stepBack();
        CHECK(snapshot() == seen[i - 1]);
    }
    CHECK(!h.getState(val::object())["canStepBack"].as<bool>());
}

static void testHeapHandlesEmptyInput() {
    beginTest("値が1つも無くても落ちない");

    HeapVisualizer h;
    h.load("setValues", "");
    h.runToEnd();
    val s = h.getState(val::object());
    CHECK_EQ(s["nodeCount"].as<int>(), 0);
    CHECK(s["finished"].as<bool>());
    CHECK(!s["canStepBack"].as<bool>());

    h.stepBack(); // 戻せないときに何も壊さない
    CHECK_EQ(h.getState(val::object())["nodeCount"].as<int>(), 0);
}


// ==========================================
// trie (接頭辞木) の構築
// ==========================================

struct ParsedTrie {
    int n = 0;
    std::vector<std::vector<std::pair<char, int>>> children; // 文字コード順
    std::set<int> terminals;
};

static ParsedTrie readTrie(TrieVisualizer& t) {
    val s = t.getState(val::object());
    ParsedTrie pt;
    pt.n = s["nodeCount"].as<int>();
    pt.children.assign(pt.n, {});

    val edges = s["edges"];
    int m = s["edgeCount"].as<int>();
    for (int i = 0; i < m; i++) {
        int from = (int)edges[i * GraphData::EDGE_STRIDE].as<float>();
        int to   = (int)edges[i * GraphData::EDGE_STRIDE + 1].as<float>();
        char c   = (char)(int)edges[i * GraphData::EDGE_STRIDE + 2].as<float>();
        if (from >= 0 && from < pt.n) pt.children[from].push_back({c, to});
    }

    val accepting = s["acceptingStates"];
    int k = accepting["length"].as<int>();
    for (int i = 0; i < k; i++) pt.terminals.insert(accepting[i].as<int>());
    return pt;
}

// 根から単語をたどれるか。たどり着けたら終端かどうかを返す
static int walk(const ParsedTrie& pt, const std::string& word) {
    int cur = 0;
    for (char c : word) {
        int next = -1;
        for (const auto& e : pt.children[cur]) if (e.first == c) next = e.second;
        if (next < 0) return -1;
        cur = next;
    }
    return cur;
}

static ParsedTrie buildTrie(const std::string& words) {
    TrieVisualizer t;
    t.load("setWords", words);
    t.runToEnd();
    return readTrie(t);
}

static void testTrieHoldsExactlyTheInsertedWords() {
    beginTest("入れた単語だけが終端として辿れる");

    ParsedTrie pt = buildTrie("to tea ten ted i in inn");

    // 入れた単語はすべて終端に届く
    for (const char* w : {"to", "tea", "ten", "ted", "i", "in", "inn"}) {
        int node = walk(pt, w);
        g_checks++;
        if (node < 0 || pt.terminals.count(node) == 0) {
            reportFailure(std::string(w) + " が終端として辿れない");
        }
    }

    // 接頭辞だけの並びは終端ではない
    for (const char* w : {"t", "te", "tea?"}) {
        int node = walk(pt, w);
        CHECK(node < 0 || pt.terminals.count(node) == 0);
    }
    // 入れていない単語は辿れない
    CHECK(walk(pt, "top") < 0);
    CHECK(walk(pt, "z") < 0);
}

static void testTrieSharesCommonPrefixes() {
    beginTest("共通の接頭辞は1本にまとまる");

    // to / tea / ten / ted は t を共有し、tea / ten / ted は te まで共有する。
    // 節点は 根 + t,o + e,a,n,d = 7 個で足りる。
    ParsedTrie pt = buildTrie("to tea ten ted");
    CHECK_EQ(pt.n, 7);

    int t = walk(pt, "t");
    CHECK(t > 0);
    CHECK_EQ(walk(pt, "te"), walk(pt, "te")); // 同じ節点に着く
    CHECK_EQ((int)pt.children[walk(pt, "te")].size(), 3); // a / n / d

    // 同じ単語を2回入れても節点は増えない
    ParsedTrie twice = buildTrie("to tea ten ted to tea");
    CHECK_EQ(twice.n, pt.n);
}

static void testTrieChildrenAreInAlphabeticalOrder() {
    beginTest("子は辞書順に並ぶ");

    // 辺の3列目は文字コード。TreeLayout がこの値で並べ替えるので、
    // 挿入した順ではなく辞書順に左から並ぶ。
    TrieVisualizer t;
    t.load("setWords", "tc ta tb");
    t.runToEnd();
    for (int i = 0; i < 400; i++) t.prepare();

    val s = t.getState(val::object());
    val nodes = s["nodes"];
    ParsedTrie pt = readTrie(t);

    int tn = walk(pt, "t");
    CHECK(tn > 0);
    std::vector<std::pair<char, float>> byChar;
    for (const auto& e : pt.children[tn]) {
        byChar.push_back({e.first, nodes[e.second * GraphData::NODE_STRIDE].as<float>()});
    }
    std::sort(byChar.begin(), byChar.end());
    CHECK_EQ((int)byChar.size(), 3);
    for (std::size_t i = 1; i < byChar.size(); i++) {
        g_checks++;
        if (!(byChar[i - 1].second < byChar[i].second)) {
            reportFailure(std::string(1, byChar[i - 1].first) + " が " +
                          std::string(1, byChar[i].first) + " より右に描かれている");
        }
    }
}

static void testTrieStepMatchesRunToEnd() {
    beginTest("step を繰り返した結果と runToEnd の結果が一致する");

    const char* inputs[] = { "to tea ten", "a", "aa ab", "" };
    for (const char* in : inputs) {
        TrieVisualizer stepwise;
        stepwise.load("setWords", in);
        int guard = 0;
        while (stepwise.step() && guard++ < 2000) {}

        TrieVisualizer atOnce;
        atOnce.load("setWords", in);
        atOnce.runToEnd();

        ParsedTrie a = readTrie(stepwise), b = readTrie(atOnce);
        CHECK_EQ(a.n, b.n);
        CHECK(a.children == b.children);
        CHECK(a.terminals == b.terminals);
    }
}

static void testTrieStepBackReturnsToPreviousState() {
    beginTest("stepBack で1手前の状態に戻る");

    TrieVisualizer t;
    t.load("setWords", "to tea");

    auto snapshot = [&]() {
        val s = t.getState(val::object());
        return std::make_tuple(s["nodeCount"].as<int>(), s["cursor"].as<int>(),
                               s["pending"].as<int>(), s["prefix"].as<std::string>());
    };

    std::vector<decltype(snapshot())> seen;
    seen.push_back(snapshot());
    CHECK(!t.getState(val::object())["canStepBack"].as<bool>());

    int guard = 0;
    while (t.step() && guard++ < 2000) seen.push_back(snapshot());

    for (int i = (int)seen.size() - 1; i >= 1; i--) {
        t.stepBack();
        CHECK(snapshot() == seen[i - 1]);
    }
    CHECK(!t.getState(val::object())["canStepBack"].as<bool>());
}

static void testTrieHandlesEmptyInput() {
    beginTest("単語が1つも無くても根だけが残る");

    TrieVisualizer t;
    t.load("setWords", "");
    t.runToEnd();
    val s = t.getState(val::object());
    CHECK_EQ(s["nodeCount"].as<int>(), 1); // 根だけ
    CHECK_EQ(s["edgeCount"].as<int>(), 0);
    CHECK(s["finished"].as<bool>());

    t.stepBack(); // 戻せないときに何も壊さない
    CHECK_EQ(t.getState(val::object())["nodeCount"].as<int>(), 1);
}


// ==========================================
// ハフマン木の構築
// ==========================================

struct ParsedHuffman {
    int n = 0;
    std::vector<float> weights;
    std::vector<std::string> labels;
    std::vector<std::vector<std::pair<char, int>>> children; // (0 or 1, 子)
    int root = -1;
};

static ParsedHuffman readHuffman(HuffmanVisualizer& h) {
    val s = h.getState(val::object());
    ParsedHuffman ph;
    ph.n = s["nodeCount"].as<int>();
    ph.root = s["startNodeIndex"].as<int>();

    val nodes = s["nodes"];
    for (int i = 0; i < ph.n; i++) {
        ph.weights.push_back(nodes[i * GraphData::NODE_STRIDE + 2].as<float>());
    }
    val ls = s["nodeLabels"];
    for (int i = 0; i < ph.n; i++) ph.labels.push_back(ls[i].as<std::string>());

    ph.children.assign(ph.n, {});
    val edges = s["edges"];
    int m = s["edgeCount"].as<int>();
    for (int i = 0; i < m; i++) {
        int from = (int)edges[i * GraphData::EDGE_STRIDE].as<float>();
        int to   = (int)edges[i * GraphData::EDGE_STRIDE + 1].as<float>();
        char bit = (char)(int)edges[i * GraphData::EDGE_STRIDE + 2].as<float>();
        if (from >= 0 && from < ph.n) ph.children[from].push_back({bit, to});
    }
    return ph;
}

static ParsedHuffman buildHuffman(const std::string& text) {
    HuffmanVisualizer h;
    h.load("setText", text);
    h.runToEnd();
    return readHuffman(h);
}

// 葉ごとの符号長を集める
static void collectDepths(const ParsedHuffman& ph, int node, int depth,
                          std::map<std::string, int>& out) {
    if (node < 0 || node >= ph.n || depth > 60) return;
    if (ph.children[node].empty()) { out[ph.labels[node]] = depth; return; }
    for (const auto& e : ph.children[node]) collectDepths(ph, e.second, depth + 1, out);
}

static void testHuffmanBuildsOneTreeFromTheForest() {
    beginTest("森が1本の木にまとまる");

    ParsedHuffman ph = buildHuffman("abracadabra");
    // a:5 b:2 r:2 c:1 d:1 の5種類。葉5 + 内部4 = 9 節点
    CHECK_EQ(ph.n, 9);
    CHECK(ph.root >= 0);

    // 根の重みは入力の文字数と一致する
    CHECK_EQ(ph.weights[ph.root], 11.0f);

    // 内部の節点はちょうど2つの子を持つ
    int leaves = 0;
    for (int i = 0; i < ph.n; i++) {
        if (ph.children[i].empty()) leaves++;
        else CHECK_EQ((int)ph.children[i].size(), 2);
    }
    CHECK_EQ(leaves, 5);
}

static void testHuffmanGivesShorterCodesToFrequentCharacters() {
    beginTest("出現回数が多い文字ほど符号が短い");

    ParsedHuffman ph = buildHuffman("abracadabra");
    std::map<std::string, int> depth;
    collectDepths(ph, ph.root, 0, depth);

    CHECK_EQ((int)depth.size(), 5);
    // a(5) は b(2) r(2) より短く、b r は c(1) d(1) より短い
    CHECK(depth["a"] < depth["b"]);
    CHECK(depth["a"] < depth["r"]);
    CHECK(depth["b"] <= depth["c"]);
    CHECK(depth["r"] <= depth["d"]);

    // 符号長 x 出現回数 の合計が、素朴な固定長より短い
    std::map<std::string, int> freq = {{"a", 5}, {"b", 2}, {"r", 2}, {"c", 1}, {"d", 1}};
    int huffmanBits = 0;
    for (const auto& d : depth) huffmanBits += d.second * freq[d.first];
    CHECK(huffmanBits < 11 * 3); // 5種類なら固定長で3ビット要る
}

static void testHuffmanNodesDoNotOverlap() {
    beginTest("節点どうしが重ならない");

    // 途中は森になり、木を横に並べる。並べ方が崩れると節点が重なる。
    HuffmanVisualizer h;
    h.load("setText", "abracadabra");
    h.runToEnd();
    for (int i = 0; i < 600; i++) h.prepare();

    val s = h.getState(val::object());
    val nodes = s["nodes"];
    int n = s["nodeCount"].as<int>();
    const float MIN_GAP = 40.0f; // 描画側の頂点の半径は 20
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            float dx = nodes[i * GraphData::NODE_STRIDE].as<float>()
                     - nodes[j * GraphData::NODE_STRIDE].as<float>();
            float dy = nodes[i * GraphData::NODE_STRIDE + 1].as<float>()
                     - nodes[j * GraphData::NODE_STRIDE + 1].as<float>();
            float d = std::sqrt(dx * dx + dy * dy);
            g_checks++;
            if (d < MIN_GAP) {
                reportFailure("節点 " + std::to_string(i) + " と " + std::to_string(j) +
                              " が " + std::to_string(d) + " しか離れていない");
            }
        }
    }
}

static void testHuffmanEdgesCarryCodeBits() {
    beginTest("枝が 0 と 1 になっている");

    ParsedHuffman ph = buildHuffman("abracadabra");
    for (int i = 0; i < ph.n; i++) {
        if (ph.children[i].empty()) continue;
        std::set<char> bits;
        for (const auto& e : ph.children[i]) bits.insert(e.first);
        CHECK(bits == std::set<char>({'0', '1'}));
    }
}

static void testHuffmanStepMatchesRunToEnd() {
    beginTest("step を繰り返した結果と runToEnd の結果が一致する");

    const char* inputs[] = { "abracadabra", "aabbc", "xyz", "aaaa", "" };
    for (const char* in : inputs) {
        HuffmanVisualizer stepwise;
        stepwise.load("setText", in);
        int guard = 0;
        while (stepwise.step() && guard++ < 2000) {}

        HuffmanVisualizer atOnce;
        atOnce.load("setText", in);
        atOnce.runToEnd();

        ParsedHuffman a = readHuffman(stepwise), b = readHuffman(atOnce);
        CHECK_EQ(a.n, b.n);
        CHECK(a.weights == b.weights);
        CHECK(a.children == b.children);
    }
}

static void testHuffmanStepBackReturnsToPreviousState() {
    beginTest("stepBack で1手前の状態に戻る");

    HuffmanVisualizer h;
    h.load("setText", "aabbc");

    auto snapshot = [&]() {
        val s = h.getState(val::object());
        return std::make_tuple(s["nodeCount"].as<int>(), s["rootCount"].as<int>(),
                               s["selectedA"].as<int>(), s["selectedB"].as<int>());
    };

    std::vector<decltype(snapshot())> seen;
    seen.push_back(snapshot());
    CHECK(!h.getState(val::object())["canStepBack"].as<bool>());

    int guard = 0;
    while (h.step() && guard++ < 2000) seen.push_back(snapshot());

    for (int i = (int)seen.size() - 1; i >= 1; i--) {
        h.stepBack();
        CHECK(snapshot() == seen[i - 1]);
    }
    CHECK(!h.getState(val::object())["canStepBack"].as<bool>());
}

static void testHuffmanHandlesTinyInput() {
    beginTest("文字が1種類しかなくても、空でも落ちない");

    ParsedHuffman one = buildHuffman("aaaa");
    CHECK_EQ(one.n, 1); // 葉が1つだけ。繋ぐ相手がいない
    CHECK_EQ(one.weights[0], 4.0f);

    HuffmanVisualizer empty;
    empty.load("setText", "");
    empty.runToEnd();
    val s = empty.getState(val::object());
    CHECK_EQ(s["nodeCount"].as<int>(), 0);
    CHECK(s["finished"].as<bool>());

    empty.stepBack(); // 戻せないときに何も壊さない
    CHECK_EQ(empty.getState(val::object())["nodeCount"].as<int>(), 0);
}


// ==========================================
// AVL 木の構築
// ==========================================

struct ParsedAvl {
    int n = 0;
    int root = -1;
    std::vector<int> keys;
    std::vector<int> left, right; // 辺の3列目 (0 が左 / 1 が右) から復元する
};

static ParsedAvl readAvl(AvlVisualizer& a) {
    val s = a.getState(val::object());
    ParsedAvl pa;
    pa.n = s["nodeCount"].as<int>();
    pa.root = s["startNodeIndex"].as<int>();
    pa.left.assign(pa.n, -1);
    pa.right.assign(pa.n, -1);

    val ls = s["nodeLabels"];
    for (int i = 0; i < pa.n; i++) pa.keys.push_back(std::stoi(ls[i].as<std::string>()));

    val edges = s["edges"];
    int m = s["edgeCount"].as<int>();
    for (int i = 0; i < m; i++) {
        int from = (int)edges[i * GraphData::EDGE_STRIDE].as<float>();
        int to   = (int)edges[i * GraphData::EDGE_STRIDE + 1].as<float>();
        float side = edges[i * GraphData::EDGE_STRIDE + 2].as<float>();
        if (from < 0 || from >= pa.n) continue;
        if (side < 0.5f) pa.left[from] = to; else pa.right[from] = to;
    }
    return pa;
}

static int avlHeight(const ParsedAvl& pa, int node, int depth = 0) {
    if (node < 0 || depth > 60) return 0;
    return 1 + std::max(avlHeight(pa, pa.left[node], depth + 1),
                        avlHeight(pa, pa.right[node], depth + 1));
}

static void avlInorder(const ParsedAvl& pa, int node, std::vector<int>& out, int depth = 0) {
    if (node < 0 || depth > 60) return;
    avlInorder(pa, pa.left[node], out, depth + 1);
    out.push_back(pa.keys[node]);
    avlInorder(pa, pa.right[node], out, depth + 1);
}

// すべての節点で |左の高さ - 右の高さ| <= 1 か
static bool avlIsBalanced(const ParsedAvl& pa, int node, int depth = 0) {
    if (node < 0 || depth > 60) return true;
    int diff = avlHeight(pa, pa.left[node]) - avlHeight(pa, pa.right[node]);
    if (diff < -1 || diff > 1) return false;
    return avlIsBalanced(pa, pa.left[node], depth + 1) &&
           avlIsBalanced(pa, pa.right[node], depth + 1);
}

static ParsedAvl buildAvl(const std::string& values) {
    AvlVisualizer a;
    a.load("setValues", values);
    a.runToEnd();
    return readAvl(a);
}

static void testAvlStaysBalanced() {
    beginTest("挿入し終えた木がどの節点でも釣り合っている");

    const char* inputs[] = {
        "10 20 30 40 50 25",
        "1 2 3 4 5 6 7 8 9 10",   // 昇順。二分探索木なら一直線になる
        "10 9 8 7 6 5 4 3 2 1",   // 降順
        "50 30 70 20 40 60 80",
        "42",
    };
    for (const char* in : inputs) {
        ParsedAvl pa = buildAvl(in);
        g_checks++;
        if (!avlIsBalanced(pa, pa.root)) {
            reportFailure(std::string(in) + ": 釣り合っていない節点がある");
        }
        // 中順に辿ると昇順。回転しても二分探索木の性質は保たれる
        std::vector<int> order;
        avlInorder(pa, pa.root, order);
        std::vector<int> sorted = order;
        std::sort(sorted.begin(), sorted.end());
        CHECK(order == sorted);
    }
}

static void testAvlKeepsHeightLow() {
    beginTest("昇順に入れても高さが伸びきらない");

    // 二分探索木なら 10 段の一直線になるところ。AVL は 4 段に収まる。
    ParsedAvl pa = buildAvl("1 2 3 4 5 6 7 8 9 10");
    CHECK_EQ(pa.n, 10);
    int h = avlHeight(pa, pa.root);
    g_checks++;
    if (h > 4) reportFailure("高さが " + std::to_string(h) + " 段ある (4 段以下のはず)");
}

static void testAvlRotationCases() {
    beginTest("4通りの回転がどれも起きる");

    // それぞれの形を最小の3つで作る
    struct Case { const char* values; const char* name; int rootKey; };
    const Case cases[] = {
        {"30 20 10", "右回転 (左左)",   20},
        {"10 20 30", "左回転 (右右)",   20},
        {"30 10 20", "左右",           20},
        {"10 30 20", "右左",           20},
    };
    for (const Case& c : cases) {
        ParsedAvl pa = buildAvl(c.values);
        CHECK_EQ(pa.n, 3);
        g_checks++;
        if (pa.root < 0 || pa.keys[pa.root] != c.rootKey) {
            reportFailure(std::string(c.name) + ": 根が " +
                          (pa.root < 0 ? std::string("無い") : std::to_string(pa.keys[pa.root])) +
                          " になっている (" + std::to_string(c.rootKey) + " のはず)");
        }
        CHECK_EQ(avlHeight(pa, pa.root), 2);
    }
}

static void testAvlIgnoresDuplicates() {
    beginTest("同じ値を2回入れても節点が増えない");

    ParsedAvl pa = buildAvl("50 30 70 30 50 70");
    CHECK_EQ(pa.n, 3);
}

static void testAvlStepMatchesRunToEnd() {
    beginTest("step を繰り返した結果と runToEnd の結果が一致する");

    const char* inputs[] = { "10 20 30 40 50 25", "1 2 3", "9 9 9", "" };
    for (const char* in : inputs) {
        AvlVisualizer stepwise;
        stepwise.load("setValues", in);
        int guard = 0;
        while (stepwise.step() && guard++ < 3000) {}

        AvlVisualizer atOnce;
        atOnce.load("setValues", in);
        atOnce.runToEnd();

        ParsedAvl a = readAvl(stepwise), b = readAvl(atOnce);
        CHECK_EQ(a.n, b.n);
        CHECK_EQ(a.root, b.root);
        CHECK(a.keys == b.keys);
        CHECK(a.left == b.left);
        CHECK(a.right == b.right);
    }
}

static void testAvlStepBackReturnsToPreviousState() {
    beginTest("stepBack で1手前の状態に戻る");

    AvlVisualizer a;
    a.load("setValues", "10 20 30 40");

    auto snapshot = [&]() {
        val s = a.getState(val::object());
        ParsedAvl pa = readAvl(a);
        return std::make_tuple(s["nodeCount"].as<int>(), s["cursor"].as<int>(),
                               s["pending"].as<int>(), pa.left, pa.right);
    };

    std::vector<decltype(snapshot())> seen;
    seen.push_back(snapshot());
    CHECK(!a.getState(val::object())["canStepBack"].as<bool>());

    int guard = 0;
    while (a.step() && guard++ < 3000) seen.push_back(snapshot());

    for (int i = (int)seen.size() - 1; i >= 1; i--) {
        a.stepBack();
        CHECK(snapshot() == seen[i - 1]);
    }
    CHECK(!a.getState(val::object())["canStepBack"].as<bool>());
}

static void testAvlHandlesEmptyInput() {
    beginTest("値が1つも無くても落ちない");

    AvlVisualizer a;
    a.load("setValues", "");
    a.runToEnd();
    val s = a.getState(val::object());
    CHECK_EQ(s["nodeCount"].as<int>(), 0);
    CHECK(s["finished"].as<bool>());

    a.stepBack();
    CHECK_EQ(a.getState(val::object())["nodeCount"].as<int>(), 0);
}

// ==========================================

int main(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--verbose" || a == "-v") g_verbose = true;
    }

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

    beginSection("Graph");
    testNoSelfLoopWhenDisallowed();
    testSelfLoopAppearsWhenAllowed();
    testNoMultiEdgeWhenDisallowed();
    testEdgeCountNeverExceedsRequest();
    testCompleteGraphEdgeCount();
    testNodeCountIsClamped();
    testCustomGraphParsing();
    testCustomGraphOptionalWeight();
    testCustomGraphRejectsOutOfRangeVertices();
    testCustomGraphIgnoresJunkLines();
    testLayoutProducesFiniteCoordinates();
    testLayoutDoesNotCollapseNodes();
    testPrepareIsIdempotentOnceStable();
    testGraphHasNoAlgorithmStep();
    testGraphTextOnlyWhenRequested();
    testColorChannelReachesTheView();
    testResetColors();
    testColorSettersIgnoreOutOfRange();
    testGenerationChangesOnRebuild();
    testAutomatonIsAlwaysDirected();
    testAutomatonStartAndAcceptingStates();
    testAutomatonDropsStaleStatesOnRegenerate();

    beginSection("BFS / DFS");
    testTraversalVisitsExactlyTheReachableSet();
    testTraversalVisitsEachVertexOnce();
    testBfsFindsShortestPath();
    testPathIsActuallyWalkable();
    testNoPathWhenUnreachable();
    testDfsGoesDeepBeforeWide();
    testStepCountIsBounded();
    testStepBackReturnsToInitialState();
    testStepBackRestoresEveryIntermediateState();
    testRunToEndMatchesRepeatedStep();
    testTraversalColorsNodesAndEdges();
    testTraversalResetsWhenGraphChanges();
    testTraversalHandlesInvalidStart();
    testStartEqualsGoal();

    beginSection("Dijkstra");
    testDijkstraDistancesMatchReference();
    testDijkstraPrefersCheaperDetour();
    testDijkstraReparentsOnRelaxation();
    testDijkstraSettlesInDistanceOrder();
    testDijkstraUnreachableGoal();
    testDijkstraStepBackRestoresState();
    testDijkstraReportsNegativeEdges();

    beginSection("頂点の重み");
    testNodeWeightsFromText();
    testNodeWeightsOmittedIsZero();
    testNodeWeightsRoundTripThroughText();
    testDijkstraDoesNotDestroyNodeWeights();

    beginSection("連結なグラフの生成");
    testConnectedOptionReachesEveryVertex();
    testWithoutConnectedOptionItCanBeDisconnected();
    testConnectedRaisesEdgeCountToSpanningTree();
    testConnectedKeepsRequestedEdgeCount();
    testConnectedWithoutMultiEdgeHasNoDuplicates();
    testConnectedWithTinyGraphs();
    testConnectedGraphIsFullyTraversed();

    beginSection("円形配置の並び順");
    testCircularOrderPlacesEveryVertexOnce();
    testCircularOrderHandlesEdgeCases();
    testSparseStructuresKeepForceLayout();
    testDenseStructuresUseCircularLayout();
    testCircularOnlyForNearCompleteGraphs();

    beginSection("重み付き / 重み無し");
    testUnweightedGraphHasNoWeightColumn();
    testCustomHeaderDoesNotEatTheBody();
    testNodeWeightLineDoesNotEatAnEdge();
    testDijkstraOnUnweightedCountsEdges();

    beginSection("DFA");
    testDfaAcceptsAndRejects();
    testDfaStepMatchesRunToEnd();
    testDfaStepBackReturnsToPreviousState();
    testDfaStopsWhenTransitionIsMissing();
    testDfaDetectsNondeterminism();
    testDfaTextRoundTrip();
    testGeneratedDfaIsDeterministicAndPartial();
    testUndefinedTransitionIsARejection();

    beginSection("木の配置");
    testTreeDepthBecomesY();
    testTreeParentIsCenteredOverChildren();
    testTreeNodesDoNotOverlap();
    testForestIsPlacedSideBySide();
    testTreeLayoutHandlesEdgeCases();
    testTreeLayoutEasesToTarget();

    beginSection("二分探索木");
    testBstInorderIsSorted();
    testBstDrawsSmallerValuesOnTheLeft();
    testBstIgnoresDuplicates();
    testBstStepMatchesRunToEnd();
    testBstStepBackReturnsToPreviousState();
    testBstHandlesEmptyInput();
    testGeneratedValuesHaveNoDuplicates();

    beginSection("ヒープの構築");
    testHeapConditionHolds();
    testMinHeapReversesTheOrder();
    testHeapShapeIsCompleteBinaryTree();
    testSwapMovesNodesInsteadOfJumping();
    testHeapStepMatchesRunToEnd();
    testHeapStepBackReturnsToPreviousState();
    testHeapHandlesEmptyInput();

    beginSection("trie の構築");
    testTrieHoldsExactlyTheInsertedWords();
    testTrieSharesCommonPrefixes();
    testTrieChildrenAreInAlphabeticalOrder();
    testTrieStepMatchesRunToEnd();
    testTrieStepBackReturnsToPreviousState();
    testTrieHandlesEmptyInput();

    beginSection("ハフマン木の構築");
    testHuffmanBuildsOneTreeFromTheForest();
    testHuffmanGivesShorterCodesToFrequentCharacters();
    testHuffmanNodesDoNotOverlap();
    testHuffmanEdgesCarryCodeBits();
    testHuffmanStepMatchesRunToEnd();
    testHuffmanStepBackReturnsToPreviousState();
    testHuffmanHandlesTinyInput();

    beginSection("AVL 木の構築");
    testAvlStaysBalanced();
    testAvlKeepsHeightLow();
    testAvlRotationCases();
    testAvlIgnoresDuplicates();
    testAvlStepMatchesRunToEnd();
    testAvlStepBackReturnsToPreviousState();
    testAvlHandlesEmptyInput();

    if (g_failures == 0) {
        std::cout << "core: OK (" << g_checks << " checks)" << std::endl;
        return 0;
    }
    std::cerr << std::endl << "core: FAILED " << g_failures << " / "
              << g_checks << " checks" << std::endl;
    return 1;
}
