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

#include "../cpp/include/Brainfuck.hpp"
#include "../cpp/include/GraphVisualizer.hpp"
#include "../cpp/include/AutomatonVisualizer.hpp"

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
    iss >> pg.v >> pg.declaredE;
    for (int i = 0; i < pg.declaredE; i++) {
        int from, to;
        float w;
        if (!(iss >> from >> to >> w)) break;
        pg.edges.push_back({from, to});
        pg.weights.push_back(w);
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
                g_checks++; g_failures++;
                std::cerr << "  FAIL [" << g_currentTest << "] 自己ループ " << e.first << std::endl;
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
                    g_checks++; g_failures++;
                    std::cerr << "  FAIL [" << g_currentTest << "] 無向で重複 "
                              << key.first << "-" << key.second << std::endl;
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
                    g_checks++; g_failures++;
                    std::cerr << "  FAIL [" << g_currentTest << "] 有向で重複 "
                              << e.first << "->" << e.second << std::endl;
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
        g.load("horizontal", "custom 1\n500 0\n");
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
    g.load("horizontal", "custom 1\n4 3\n0 1 5\n1 2 7\n2 3 9\n");
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
    beginTest("重みを省略したテキストも読める");

    GraphVisualizer g;
    g.load("horizontal", "custom 1\n3 2\n0 1\n1 2\n");
    ParsedGraph pg = readGraph(g);

    CHECK_EQ(pg.v, 3);
    CHECK_EQ((int)pg.edges.size(), 2);
    if (pg.weights.size() == 2) {
        CHECK_EQ(pg.weights[0], 0.0f);
        CHECK_EQ(pg.weights[1], 0.0f);
    }
}

static void testCustomGraphRejectsOutOfRangeVertices() {
    beginTest("範囲外の頂点番号を含む辺は捨てられる");

    // ここで弾かないと隣接リスト構築で範囲外アクセスになる
    GraphVisualizer g;
    g.load("horizontal", "custom 1\n3 4\n0 1\n1 99\n-5 2\n2 0\n");
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
    g.load("horizontal", "custom 1\n3 2\n\n0 1 3\n\nhello\n1 2 4\n");
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
        "custom 1\n6 2\n0 1\n2 3\n",   // 非連結（孤立点あり）
        "custom 1\n1 0\n",             // 頂点1個
        "custom 1\n2 1\n0 1\n",        // 直線
        "custom 1\n5 0\n",             // 全部孤立
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
        if (!ok) {
            g_failures++;
            std::cerr << "  FAIL [" << g_currentTest << "] 座標が有限でない: " << c << std::endl;
        }
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
    if (worst < 10.0f) {
        g_failures++;
        std::cerr << "  FAIL [" << g_currentTest << "] 最接近距離 " << worst << std::endl;
    }
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

    CHECK(!g.getState(val::object()).hasOwnProperty("graphText"));

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
    g.load("horizontal", "custom 1\n4 3\n0 1\n1 2\n2 3\n");

    g.paintNode(0, NODE_START);
    g.paintNode(1, NODE_FRONTIER);
    g.paintEdge(0, EDGE_PATH);
    g.paintEdge(2, EDGE_TREE);

    val state = g.getState(val::object());
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
    g.load("horizontal", "custom 1\n3 2\n0 1\n1 2\n");
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
    g.load("horizontal", "custom 1\n2 1\n0 1\n");
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
    int before = g.getState(val::object())["generation"].as<int>();

    g.load("horizontal", "random 6 8 1 0 0 0");
    int after = g.getState(val::object())["generation"].as<int>();
    CHECK(after > before);

    // レイアウトの向きを変えただけでは作り直さない
    g.load("vertical", "");
    CHECK_EQ(g.getState(val::object())["generation"].as<int>(), after);
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
    CHECK(a.getState(val::object())["isDirected"].as<bool>());
    CHECK(a.getState(val::object())["isAutomaton"].as<bool>());
}

static void testAutomatonStartAndAcceptingStates() {
    beginTest("初期状態と受理状態を C++ が保持する");

    AutomatonVisualizer a;
    a.load("horizontal", "custom 1\n5 2\n0 1\n1 2\n");

    a.load("setStartNode", "2");
    CHECK_EQ(a.getState(val::object())["startNodeIndex"].as<int>(), 2);

    // 範囲外は -1 に落とす
    a.load("setStartNode", "99");
    CHECK_EQ(a.getState(val::object())["startNodeIndex"].as<int>(), -1);

    // カンマ区切りを受け付け、範囲外は捨てる
    a.load("setAccepting", "1, 3, 99");
    val accepting = a.getState(val::object())["acceptingStates"];
    CHECK_EQ(accepting["length"].as<int>(), 2);
    if (accepting["length"].as<int>() == 2) {
        CHECK_EQ(accepting[0].as<int>(), 1);
        CHECK_EQ(accepting[1].as<int>(), 3);
    }
}

static void testAutomatonDropsStaleStatesOnRegenerate() {
    beginTest("グラフを作り直すと範囲外になった状態指定が消える");

    AutomatonVisualizer a;
    a.load("horizontal", "custom 1\n10 0\n");
    a.load("setStartNode", "8");
    a.load("setAccepting", "7, 9");
    CHECK_EQ(a.getState(val::object())["startNodeIndex"].as<int>(), 8);

    // 3頂点に作り直すと 7,8,9 は存在しなくなる
    a.load("horizontal", "custom 1\n3 0\n");
    CHECK_EQ(a.getState(val::object())["startNodeIndex"].as<int>(), -1);
    CHECK_EQ(a.getState(val::object())["acceptingStates"]["length"].as<int>(), 0);
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

    std::cout << std::endl << "=== Graph ===" << std::endl;
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

    std::cout << std::endl;
    if (g_failures == 0) {
        std::cout << "OK: " << g_checks << " checks passed" << std::endl;
        return 0;
    }
    std::cout << "FAILED: " << g_failures << " / " << g_checks
              << " checks failed" << std::endl;
    return 1;
}
