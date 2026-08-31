// 出荷される public/wasm/core.js に対するスモークテスト。
//
// C++ を書き換えたのに WASM を再ビルドし忘れる、あるいは embind の
// バインディング定義が壊れる、という事故を検出する。
// Docker は不要で1秒で終わるので、コミット前に必ず流す。
//
//   npm run test:smoke
//
import { createRequire } from 'node:module';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

// core.js は emcc が吐く CommonJS だが、ルートの package.json が
// "type": "module" なので Node は .js を ESM として読み、module.exports が効かない。
// 出荷物に package.json を置いて汚したくないので、ここで CommonJS として評価する。
// core.js は node 環境で __dirname から core.wasm を探すため、それも渡す。
function loadCore() {
    const coreDir = join(dirname(fileURLToPath(import.meta.url)), '..', 'public', 'wasm');
    const corePath = join(coreDir, 'core.js');
    const src = readFileSync(corePath, 'utf8');

    const mod = { exports: {} };
    const fn = new Function('module', 'exports', 'require', '__dirname', '__filename', src);
    fn(mod, mod.exports, createRequire(corePath), coreDir, corePath);
    return mod.exports;
}

// 成功したときは1行だけ出す。読む必要があるのは失敗したときだけ。
// どこで落ちたか追いたいときは --verbose を付ける。
const verbose = process.argv.includes('--verbose') || process.argv.includes('-v');

let failures = 0;
let checks = 0;
let currentSection = '';
let sectionReported = false;

function section(name) {
    currentSection = name;
    sectionReported = false;
    if (verbose) console.log(`- ${name}`);
}

// 失敗したセクションの名前を1回だけ出す
function failHeader() {
    if (sectionReported) return;
    sectionReported = true;
    console.error(`
FAIL: ${currentSection}`);
}

function check(label, cond) {
    checks++;
    if (!cond) {
        failures++;
        failHeader();
        console.error(`  ${label}`);
    }
}

function checkEq(label, actual, expected) {
    checks++;
    if (actual !== expected) {
        failures++;
        failHeader();
        console.error(`  ${label}`);
        console.error(`    expected: ${JSON.stringify(expected)}`);
        console.error(`    actual  : ${JSON.stringify(actual)}`);
    }
}

const createVisualizerModule = loadCore();
section('モジュールの読み込み');
check('core.js が createVisualizerModule をエクスポートする', typeof createVisualizerModule === 'function');
if (typeof createVisualizerModule !== 'function') {
    console.error('\nFAILED: core.js のエクスポートが読めない。');
    process.exit(1);
}
const Module = await createVisualizerModule();

check('VisualizerEngine がエクスポートされている', typeof Module.VisualizerEngine === 'function');
if (!Module.VisualizerEngine) {
    console.error('\nFAILED: VisualizerEngine が見つからない。WASM を再ビルドしてください。');
    process.exit(1);
}

const engine = new Module.VisualizerEngine();

// --- バインディングが全部生えているか ---
for (const name of ['setAlgorithm', 'load', 'prepare', 'step', 'runToEnd', 'stepBack',
                    'getState', 'getOutput', 'setBrainfuckModint']) {
    check(`${name}() がバインドされている`, typeof engine[name] === 'function');
}

// --- Brainfuck: 実際に走らせる ---
section('Brainfuck');
engine.setAlgorithm('brainfuck');
engine.setBrainfuckModint(true);
engine.load(
    '++++++++++[>+++++++>++++++++++>+++>+<<<<-]>++.>+.+++++++..+++.>++.' +
    '<<+++++++++++++++.>.+++.------.--------.>+.>.',
    ''
);
engine.runToEnd();
checkEq('Hello World が出力される', engine.getOutput(), 'Hello World!\n');

const state = engine.getState({ start: 0, range: 8 });
check('getState が tape を返す', Array.isArray(state.tape) || state.tape.length === 8);
checkEq('tape の長さ', state.tape.length, 8);
check('getState が pc を返す', typeof state.pc === 'number');
check('getState が ptr を返す', typeof state.ptr === 'number');
check('getState が isError を返す', typeof state.isError === 'boolean');
check('getState が interrupted を返す', typeof state.interrupted === 'boolean');
checkEq('正常終了は中断扱いにならない', state.interrupted, false);

// --- 停止しないプログラムで固まらない ---
section('停止しないプログラムの打ち切り');
engine.load('+[]', '');
const started = Date.now();
engine.runToEnd();
const elapsed = Date.now() - started;
const infinite = engine.getState({ start: 0, range: 4 });
checkEq('ステップ上限で中断される', infinite.interrupted, true);
check(`妥当な時間で返る (実測 ${elapsed}ms)`, elapsed < 30000);

// --- ステップ実行とステップバック ---
section('ステップ実行 / ステップバック');
engine.load('+++.', '');
engine.step();
engine.step();
const mid = engine.getState({ start: 0, range: 4 });
checkEq('2ステップで値が 2 になる', mid.tape[0].value, 2);
engine.stepBack();
const back = engine.getState({ start: 0, range: 4 });
checkEq('1ステップ戻ると値が 1 になる', back.tape[0].value, 1);

// --- グラフエンジンが生きているか ---
// C++ 側 GraphData の stride。ここがずれると描画が全部崩れる。
const NODE_STRIDE = 4;
const EDGE_STRIDE = 4;

section('Graph');
engine.setAlgorithm('graph');
engine.load('horizontal', 'random 6 8 1 0 0 0');
const g = engine.getState({});
check('nodes が返る', g.nodes !== undefined);
check('edges が返る', g.edges !== undefined);
checkEq('頂点数が指定通り', g.nodes.length / NODE_STRIDE, 6);
checkEq('辺数が指定通り', g.edges.length / EDGE_STRIDE, 8);
checkEq('頂点数の上限が公開されている', typeof g.maxNodes, 'number');
checkEq('graphText は既定では作られない', g.graphText, undefined);

const withText = engine.getState({ withText: true });
check('withText で graphText が返る', typeof withText.graphText === 'string');
checkEq('graphText の1行目が V E', withText.graphText.split('\n')[0], '6 8');

check('prepare() でレイアウトが収束する', engine.prepare());
checkEq('収束が state に反映される', engine.getState({}).layoutStable, true);
const coords = engine.getState({}).nodes;
check('座標が有限', [...coords].every(Number.isFinite));

// --- オートマトン ---
section('Automaton');
engine.setAlgorithm('automaton');
engine.load('horizontal', 'complete 4 1 0'); // 無向を指定しても有向になる
const a = engine.getState({});
checkEq('常に有向', a.isDirected, true);
checkEq('オートマトンとして報告される', a.isAutomaton, true);
checkEq('有向完全グラフの辺数', a.edges.length / EDGE_STRIDE, 12);

engine.load('setStartNode', '2');
engine.load('setAccepting', '1, 3, 99');
const a2 = engine.getState({});
checkEq('初期状態', a2.startNodeIndex, 2);
checkEq('受理状態は範囲内だけ', a2.acceptingStates.length, 2);

// --- BFS / DFS ---
section('Traversal');
engine.setAlgorithm('traversal');
// 0-1-2-3 の道と 0-3 の近道
engine.load('horizontal', 'custom 1 0 0 1\n4 4\n0 1\n1 2\n2 3\n0 3\n');
engine.load('setTraversal', 'bfs 0 3');

const t0 = engine.getState({ withProgress: true });
checkEq('開始時は始点を処理中でキューは空', t0.frontier.length, 0);
checkEq('処理中は始点', t0.current, 0);
check('進行状況は既定では返らない', engine.getState({}).frontier === undefined);
checkEq('まだ終わっていない', t0.finished, false);

engine.runToEnd();
const t1 = engine.getState({ withProgress: true });
checkEq('経路が見つかる', t1.found, true);
checkEq('BFS なので最短の2頂点', t1.path.length, 2);
checkEq('経路の始点', t1.path[0], 0);
checkEq('経路の終点', t1.path[t1.path.length - 1], 3);
check('戻れる履歴がある', t1.canStepBack);

engine.stepBack();
checkEq('戻ると未完了に戻る', engine.getState({}).finished, false);

engine.load('setTraversal', 'dfs 0 -1');
engine.runToEnd();
const t2 = engine.getState({ withProgress: true });
checkEq('DFS で全頂点を訪問', t2.visitOrder.length, 4);
checkEq('探索名が返る', t2.algorithm, 'dfs');

// ダイクストラ: 遠回りの方が軽い経路を選ぶ
engine.load('horizontal', 'custom 1 0 0 1\n4 4\n0 1 10\n1 2 10\n2 3 10\n0 3 100\n');
engine.load('setTraversal', 'dijkstra 0 3');
engine.runToEnd();
const t3 = engine.getState({ withProgress: true });
checkEq('ダイクストラが経路を見つける', t3.found, true);
checkEq('重みで最短の4頂点', t3.path.length, 4);
checkEq('経路長', t3.goalDistance, 30);
checkEq('頂点の脇の数値は距離', t3.nodeValueMode, 'distance');
check('距離が返る', Array.isArray(t3.distances));
checkEq('負の重みは無い', t3.hasNegativeEdge, false);

// 頂点の重み
engine.load('horizontal', 'custom 1 0 1 1\n3 1\n7 8 9\n0 1 2\n');
const nw = engine.getState({ withText: true });
checkEq('頂点の重みを受け取った', nw.hasNodeWeights, true);
checkEq('テキストに頂点の重みが載る', nw.graphText.split('\n')[1], '7 8 9');

if (failures === 0) {
    console.log(`smoke: OK (${checks} checks)`);
    process.exit(0);
}
console.error(`
smoke: FAILED ${failures} / ${checks} checks`);
process.exit(1);
