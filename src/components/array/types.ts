// 配列のビジュアライザで共有する型。1ページ1アルゴリズムなので、
// variant がそのままページの中身を決める。

export type ArrayVariant = 'bubble';

export const ARRAY_TITLE: Record<ArrayVariant, string> = {
    bubble: 'バブルソート',
};

/** C++ 側の setAlgorithm へ渡す名前 */
export const arrayAlgorithm = (v: ArrayVariant): string => v;

/** 既定で入れておく値。動きが分かりやすい並びにしてある */
export const defaultValues: Record<ArrayVariant, string> = {
    bubble: '5 2 9 1 7 3 8 4',
};
