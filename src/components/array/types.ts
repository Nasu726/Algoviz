// 配列のビジュアライザで共有する型。1ページ1アルゴリズムなので、
// variant がそのままページの中身を決める。

export type ArrayVariant =
    'bubble' | 'selection' | 'insertion' | 'shaker' | 'quick' | 'merge';

export const ARRAY_TITLE: Record<ArrayVariant, string> = {
    bubble: 'バブルソート',
    selection: '選択ソート',
    insertion: '挿入ソート',
    shaker: 'シェーカーソート',
    quick: 'クイックソート',
    merge: 'マージソート',
};

/** C++ 側の setAlgorithm へ渡す名前 */
export const arrayAlgorithm = (v: ArrayVariant): string => v;

/**
 * 灰色の範囲が「もう動かない」のか「並んでいるだけ」なのか。
 * 挿入ソートだけ、後から来た値が割り込むので確定ではない。
 */
// 挿入とマージの灰色は「その中では並んでいる」だけで、位置は確定していない
const ordersWithoutSettling = (v: ArrayVariant) => v === 'insertion' || v === 'merge';

export const settledLabel = (v: ArrayVariant): string =>
    ordersWithoutSettling(v) ? '並んでいる範囲' : '位置が確定した値';

export const settledCountLabel = (v: ArrayVariant): string =>
    ordersWithoutSettling(v) ? '並んでいる個数' : '位置が確定した個数';

/** 既定で入れておく値。動きが分かりやすい並びにしてある */
export const defaultValues: Record<ArrayVariant, string> = {
    bubble: '5 2 9 1 7 3 8 4',
    selection: '5 2 9 1 7 3 8 4',
    insertion: '5 2 9 1 7 3 8 4',
    // 小さい値が右端にある並び。左向きの走査の効きどころ
    shaker: '2 3 4 5 6 7 8 1',
    quick: '5 2 9 1 7 3 8 4',
    merge: '5 2 9 1 7 3 8 4',
};
