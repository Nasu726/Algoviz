// グラフ系ページで共有する型と小さなヘルパー。

/** 0xRRGGBB を CSS の色に */
export const hex = (n: number) => '#' + n.toString(16).padStart(6, '0');

/** 1ページ1アルゴリズム。ルートと1対1に対応する */
export type GraphVariant = 'bfs' | 'dfs' | 'dijkstra' | 'automaton' | 'plain';

/** 探索を行う variant か */
export const isTraversal = (v: GraphVariant): v is 'bfs' | 'dfs' | 'dijkstra' =>
    v === 'bfs' || v === 'dfs' || v === 'dijkstra';

export const VARIANT_TITLE: Record<GraphVariant, string> = {
    bfs: '幅優先探索 (BFS)',
    dfs: '深さ優先探索 (DFS)',
    dijkstra: 'ダイクストラ法',
    automaton: 'オートマトン',
    plain: 'グラフ描画',
};

/** C++ 側の setAlgorithm へ渡す名前 */
export const engineAlgorithm = (v: GraphVariant): string =>
    isTraversal(v) ? 'traversal' : v === 'automaton' ? 'automaton' : 'graph';

/** グラフの作り方と見た目の設定。実行前に決めるもの */
export interface GraphSettings {
    nodeCount: string;
    edgeCount: string;
    isDirected: boolean;
    allowSelfLoop: boolean;
    allowSameEdge: boolean;
    connected: boolean;
    useNodeWeights: boolean;
    skipExtension: boolean;
    isHorizontal: boolean;
    showWeights: boolean;
    labelType: 'index' | 'name';
    inputBuffer: string;
}

export const defaultSettings = (variant: GraphVariant): GraphSettings => ({
    nodeCount: '8',
    edgeCount: '10',
    // オートマトンは常に有向なので、既定でチェックを入れておく
    isDirected: variant === 'automaton',
    allowSelfLoop: false,
    allowSameEdge: false,
    // 探索を見せるページでは、途中で行き止まりにならない方が既定として自然
    connected: isTraversal(variant),
    useNodeWeights: false,
    skipExtension: true,
    isHorizontal: true,
    showWeights: true,
    labelType: variant === 'automaton' ? 'name' : 'index',
    inputBuffer: '',
});
