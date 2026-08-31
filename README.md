# AlgoViz

さまざまなアルゴリズムやデータ構造、機械の動作を可視化します。

[メインメニュー(一覧)](https://algoviz.nasu.uk)

## 現在利用可能なビジュアライザ

- [Brainfuck](https://algoviz.nasu.uk/brainfuck) — テープとポインタの動きを1命令ずつ追える
- [グラフ探索](https://algoviz.nasu.uk/graph) — BFS / DFS の探索過程を1手ずつ追える。オートマトンの表示にも対応

## 仕組み

アルゴリズムの論理はすべて C++ で書き、WebAssembly にコンパイルして動かします。
JavaScript 側は「画面のどこに何色の丸を描き、どう線を繋ぐか」という視覚データだけを
受け取って描画に専念します。JS はそれが BFS なのかオートマトンなのかを知りません。

```
cpp/
  main.cpp                     VisualizerEngine。embind で JS へ公開する窓口
  include/
    IVisualizer.hpp            全ビジュアライザ共通のインターフェース
    Brainfuck.hpp / src/       Brainfuck インタプリタ
    GraphData.hpp              頂点・辺の平坦な float 配列。JS とゼロコピー共有する
    GraphColors.hpp            colorId の「意味」の定義 (実際の配色は JS 側)
    GeneralGraphLayout.hpp     レイアウト計算 (MDS → Stress Majorization → 凸包パッキング)
    GraphVisualizer.hpp        グラフの生成・レイアウト・描画データ供給の基底クラス
    AutomatonVisualizer.hpp    ├ オートマトン (常に有向 + 初期状態 + 受理状態)
    TraversalVisualizer.hpp    └ BFS / DFS
src/
  types/engine.ts              WASM 境界の型定義。C++ を変えたらここも揃える
  pages/                       ページ
  components/visualizers/      PixiJS による描画
```

`step()` と `prepare()` の役割分担が設計の要です。

- `prepare()` … レイアウトの収束計算。描画ループが毎フレーム呼ぶ
- `step()` … アルゴリズムの1手。再生コントロールが呼ぶ

## 開発

必要なもの: Node.js 20+ と Docker (WASM のビルドに emscripten イメージを使う)。

emscripten のバージョンは `emscripten/emsdk:5.0.1` に固定しています。`latest` のままだと
ローカルと CI で別のコンパイラが動き、出荷する WASM が再現しなくなるためです。
C++ をリンクするので `emcc` ではなく `em++` を使います。

```bash
npm install
npm run dev
```

### WASM の再ビルド

C++ を変えたら必ず再ビルドしてコミットします。Cloudflare Pages のビルド環境に
emscripten は無いため、`public/wasm/` の成果物はリポジトリに含めます。

```bash
npm run build:wasm
```

再ビルド後は `npm run test:smoke` を流して、出荷する `core.js` が期待どおりに
動くことを確認してからコミットしてください。

### テスト

ネイティブの C++ コンパイラは使わず、本番と同じ emcc で WASM にビルドして
Node で実行します。本番と同一のコンパイラ・標準ライブラリで検証できます。

```bash
npm test --silent
```

- `npm run test:core` — C++ ロジックを直接叩くテスト (Docker が必要)
- `npm run test:smoke` — 出荷する `public/wasm/core.js` への疎通テスト (Docker 不要・1秒)

成功時は結果の1行だけを出します。失敗したテストだけが名前と詳細を出すので、
出力があれば何か壊れています。どこまで進んだか見たいときは `--verbose` を付けます。

```bash
npm run test:core -- --verbose
```

`build:wasm` と `build:test` は Windows の `npm run` (cmd.exe) 前提で `%cd%` を
使っています。POSIX シェルから直接動かす場合は `$PWD` に読み替えてください。

## ライセンス

MIT
