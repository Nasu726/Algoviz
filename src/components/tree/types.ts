// 木のビジュアライザで共有する型。1ページ1アルゴリズムなので、
// variant がそのままページの中身を決める。

export type TreeVariant = 'bst' | 'heap';

export const TREE_TITLE: Record<TreeVariant, string> = {
    bst: '二分探索木の構築',
    heap: 'ヒープの構築',
};

/** C++ 側の setAlgorithm へ渡す名前 */
export const treeAlgorithm = (v: TreeVariant): string => v;

/** 既定で入れておく値。木の形が分かりやすい並びにしてある */
export const defaultValues: Record<TreeVariant, string> = {
    bst: '50 30 70 20 40 60 80',
    heap: '20 40 30 80 50 70 60',
};
