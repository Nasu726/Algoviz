// 配列のビジュアライザで共有する型。1ページ1アルゴリズムなので、
// variant がそのままページの中身を決める。

export type ArrayVariant =
    'bubble' | 'selection' | 'insertion' | 'shaker' | 'quick' | 'merge'
    | 'linear' | 'binary'
    | 'stack' | 'queue' | 'deque';

/** 値を並べ替えるものか、値を探すものか。入力欄と凡例の言葉が変わる */
export const isSearch = (v: ArrayVariant): boolean => v === 'linear' || v === 'binary';

/** 値の並びではなく、操作の並びを入れるものか */
export const usesOps = (v: ArrayVariant): boolean =>
    v === 'stack' || v === 'queue' || v === 'deque';

export const ARRAY_TITLE: Record<ArrayVariant, string> = {
    bubble: 'バブルソート',
    selection: '選択ソート',
    insertion: '挿入ソート',
    shaker: 'シェーカーソート',
    quick: 'クイックソート',
    merge: 'マージソート',
    linear: '線形探索',
    binary: '二分探索',
    stack: 'スタック',
    queue: 'キュー',
    deque: 'デック',
};

/** C++ 側の setAlgorithm へ渡す名前 */
export const arrayAlgorithm = (v: ArrayVariant): string => v;

// 灰色の意味は3通りある。
//   ソート        … その位置が確定した
//   挿入 / マージ … その中では並んでいるだけ (後から来た値が割り込む)
//   探索          … もう見ない
const greyMeaning = (v: ArrayVariant): 'settled' | 'ordered' | 'skipped' =>
    isSearch(v) ? 'skipped'
    : v === 'insertion' || v === 'merge' ? 'ordered'
    : 'settled';

export const settledLabel = (v: ArrayVariant): string => {
    const kind = greyMeaning(v);
    return kind === 'skipped' ? 'もう見ない範囲'
        : kind === 'ordered' ? '並んでいる範囲'
        : '位置が確定した値';
};

export const settledCountLabel = (v: ArrayVariant): string => {
    const kind = greyMeaning(v);
    return kind === 'skipped' ? '見終わった個数'
        : kind === 'ordered' ? '並んでいる個数'
        : '位置が確定した個数';
};

/** 既定で入れておく値。動きが分かりやすい並びにしてある */
export const defaultValues: Record<ArrayVariant, string> = {
    bubble: '5 2 9 1 7 3 8 4',
    selection: '5 2 9 1 7 3 8 4',
    insertion: '5 2 9 1 7 3 8 4',
    // 小さい値が右端にある並び。左向きの走査の効きどころ
    shaker: '2 3 4 5 6 7 8 1',
    quick: '5 2 9 1 7 3 8 4',
    merge: '5 2 9 1 7 3 8 4',
    linear: '5 2 9 1 7 3 8 4',
    // 二分探索は並んでいることが前提
    binary: '1 2 3 4 5 7 8 9',
    stack: 'push 5 push 2 pop push 9 pop pop',
    queue: 'push 5 push 2 pop push 9 pop pop',
    deque: 'pushR 5 pushL 2 pushR 9 popL popR popL',
};

/** 探すものの既定の値。値の中に在るものにしてある */
export const defaultTarget: Record<ArrayVariant, string> = {
    bubble: '', selection: '', insertion: '', shaker: '', quick: '', merge: '',
    linear: '7', binary: '7',
    stack: '', queue: '', deque: '',
};
