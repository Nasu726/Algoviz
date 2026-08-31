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

let failures = 0;
let checks = 0;

function check(label, cond) {
    checks++;
    if (!cond) {
        failures++;
        console.error(`  FAIL: ${label}`);
    }
}

function checkEq(label, actual, expected) {
    checks++;
    if (actual !== expected) {
        failures++;
        console.error(`  FAIL: ${label}`);
        console.error(`    expected: ${JSON.stringify(expected)}`);
        console.error(`    actual  : ${JSON.stringify(actual)}`);
    }
}

console.log('=== AlgoViz wasm smoke test ===');

const createVisualizerModule = loadCore();
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
for (const name of ['setAlgorithm', 'load', 'step', 'runToEnd', 'stepBack',
                    'getState', 'getOutput', 'setBrainfuckModint']) {
    check(`${name}() がバインドされている`, typeof engine[name] === 'function');
}

// --- Brainfuck: 実際に走らせる ---
console.log('- Brainfuck');
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
console.log('- 停止しないプログラムの打ち切り');
engine.load('+[]', '');
const started = Date.now();
engine.runToEnd();
const elapsed = Date.now() - started;
const infinite = engine.getState({ start: 0, range: 4 });
checkEq('ステップ上限で中断される', infinite.interrupted, true);
check(`妥当な時間で返る (実測 ${elapsed}ms)`, elapsed < 30000);

// --- ステップ実行とステップバック ---
console.log('- ステップ実行 / ステップバック');
engine.load('+++.', '');
engine.step();
engine.step();
const mid = engine.getState({ start: 0, range: 4 });
checkEq('2ステップで値が 2 になる', mid.tape[0].value, 2);
engine.stepBack();
const back = engine.getState({ start: 0, range: 4 });
checkEq('1ステップ戻ると値が 1 になる', back.tape[0].value, 1);

// --- グラフエンジンが生きているか ---
console.log('- Graph');
engine.setAlgorithm('graph');
engine.load('horizontal', 'random 6 8');
const g = engine.getState({});
check('nodes が返る', g.nodes !== undefined);
check('edges が返る', g.edges !== undefined);
checkEq('頂点数が指定通り', g.nodes.length / 5, 6);
checkEq('辺数が指定通り', g.edges.length / 4, 8);

console.log('');
if (failures === 0) {
    console.log(`OK: ${checks} checks passed`);
    process.exit(0);
}
console.log(`FAILED: ${failures} / ${checks} checks failed`);
process.exit(1);
