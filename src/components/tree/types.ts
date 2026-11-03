// 木のビジュアライザで共有する型。1ページ1アルゴリズムなので、
// variant がそのままページの中身を決める。

export type TreeVariant = 'bst' | 'heap' | 'heapsort' | 'trie' | 'huffman' | 'avl' | 'btree' | 'segtree';

export const TREE_TITLE: Record<TreeVariant, string> = {
    bst: '二分探索木の構築',
    heap: 'ヒープの構築',
    heapsort: 'ヒープソート',
    trie: 'trie (接頭辞木) の構築',
    huffman: 'ハフマン木の構築',
    avl: 'AVL 木の構築',
    btree: 'B木の構築',
    segtree: 'セグメント木',
};

/** 値の列に加えて、操作の並びも入れる variant か */
export const usesOps = (v: TreeVariant): boolean => v === 'segtree';

/** セグメント木の集約。どれも正しいセグメント木なので選ばせる */
export type Monoid = 'sum' | 'min' | 'max';

/** 値の列ではなく単語を入れる variant か */
export const usesWords = (v: TreeVariant): boolean => v === 'trie';

/** 値の列ではなく文章を入れる variant か */
export const usesText = (v: TreeVariant): boolean => v === 'huffman';

/** C++ 側の setAlgorithm へ渡す名前 */
export const treeAlgorithm = (v: TreeVariant): string => v;

/** 既定で入れておく値。木の形が分かりやすい並びにしてある */
export const defaultValues: Record<TreeVariant, string> = {
    bst: '50 30 70 20 40 60 80',
    heap: '20 40 30 80 50 70 60',
    heapsort: '20 40 30 80 50 70 60',
    trie: 'to tea ten ted i in inn',
    huffman: 'abracadabra',
    avl: '10 20 30 40 50 25',
    btree: '10 20 30 40 50 60 70',
    segtree: '5 3 8 1 6 2 7 4',
};

/** usesOps のときに既定で入れておく操作 */
export const defaultOps = 'query 1 6\nupdate 3 9\nquery 2 8';
