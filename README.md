# AlgoViz

さまざまなアルゴリズムやデータ構造、機械の動作を可視化します。

[メインメニュー(一覧)](https://algoviz.nasu.uk)

## 現在利用可能なビジュアライザ

1ページ1アルゴリズムです。

**グラフ探索**

- [幅優先探索 (BFS)](https://algoviz.nasu.uk/graph/bfs)
- [深さ優先探索 (DFS)](https://algoviz.nasu.uk/graph/dfs)
- [ダイクストラ法](https://algoviz.nasu.uk/graph/dijkstra)

**木**

- [二分探索木の構築](https://algoviz.nasu.uk/tree/bst)
- [AVL 木の構築](https://algoviz.nasu.uk/tree/avl)
- [B木の構築](https://algoviz.nasu.uk/tree/btree)
- [ヒープの構築](https://algoviz.nasu.uk/tree/heap)
- [trie (接頭辞木) の構築](https://algoviz.nasu.uk/tree/trie)
- [ハフマン木の構築](https://algoviz.nasu.uk/tree/huffman)

**配列**

- [バブルソート](https://algoviz.nasu.uk/array/bubble)

**オートマトン**

- [決定性有限オートマトン (DFA)](https://algoviz.nasu.uk/automaton)

**その他**

- [Brainfuck](https://algoviz.nasu.uk/brainfuck)

## 設計方針

### 責任範囲の線引き

**AlgoViz が保証するのは「アルゴリズムが動く様子を1手ずつ見せること」だけ**です。
それ以外は外部の資料 (技術ブログ、教科書、Wikipedia など) が既に担っている領域なので、
持ち込みません。

判断に迷ったら「これは AlgoViz が保証すべき体験の範囲内か、外部が担う領域か」を先に問います。
線を引かないと実装が膨らみ、本来見るべき点を見落とします。

範囲の外に置くもの:

- アルゴリズムの解説文。ビジュアライザ一覧に「始点から近い順に広がる」のような説明は要らない
  (何を選ぶかは名前で分かるし、意味は外部の資料が説明している)
- 一般的な用語の定義、計算量の解説、実装のバリエーション比較

範囲の内に置くもの:

- **このページの操作方法**と、**画面に出ている色や数字が何を指すか** (ヘルプと凡例)
- そのアルゴリズム固有の、動きを見ないと分からないこと
  (「取り出した瞬間に確定する」「隣接を見終わると frames から外れる = バックトラック」など)

これは文章に限りません。UI の要素、設定項目、表示のどれについても同じ問いを当てます。

### その他

- **1ページ1アルゴリズム。** 「このページは DFS を見せるためだけのもの」と決まっている方が、
  作る側も見る側も迷わない。モードを切り替えるプルダウンは置かない
- **アルゴリズムの論理は C++、見た目は JS。** どちらに置くか迷うものは、
  「別の描画方法に差し替えても必要か」で決める。必要なら論理側 (C++)
- **デバッグ用の表示は本番に出さない。** `import.meta.env.DEV` で囲んでバンドルから落とす
- **分類ごとに決まるものは選ばせず固定する。** グラフの頂点は `0, 1, 2`、
  オートマトンの状態は `q₀, q₁, q₂`、木は節点の値。どれかを選ぶ場面は実際には無い

エージェント向けの指針は [AGENTS.md](AGENTS.md) にあります。

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
                               (節点ごとの半幅も持つ。B木は値の数だけ横に広がる)
    GraphColors.hpp            colorId の「意味」の定義 (実際の配色は JS 側)
    ILayout.hpp                配置アルゴリズムの面。向きは配置が決める
    GeneralGraphLayout.hpp     ├ 一般グラフ (MDS → Stress Majorization → 凸包パッキング)
    EasedLayout.hpp            └ 目標へイージングで寄せる配置の共通部分
    TreeLayout.hpp                ├ 木 (Reingold-Tilford)。常に上から下
    LineLayout.hpp                └ 配列 (番号順に左から一列)
    GraphVisualizer.hpp        グラフの生成・レイアウト・描画データ供給の基底クラス
    TraversalVisualizer.hpp    ├ BFS / DFS / ダイクストラ法
    AutomatonVisualizer.hpp    ├ DFA (常に有向 + 初期状態 + 受理状態 + 遷移記号)
    BstVisualizer.hpp          ├ 二分探索木の構築
    AvlVisualizer.hpp          ├ AVL 木の構築 (回転で形が変わる)
    BTreeVisualizer.hpp        ├ B木の構築 (1つの節点に値が複数入る)
    HeapVisualizer.hpp         ├ ヒープの構築
    TrieVisualizer.hpp         ├ trie の構築
    HuffmanVisualizer.hpp      └ ハフマン木の構築
    ArrayVisualizer.hpp        配列を一列に並べるものの基底 (節点の番号 = 添字)
    BubbleSortVisualizer.hpp   └ バブルソート
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
