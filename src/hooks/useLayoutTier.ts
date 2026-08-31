import { useViewportWidth } from './useViewportWidth';

// 画面幅で3段階に分ける。
//   wide   … 設定サイドバー + キャンバス + 実行サイドバー (2サイドバー)
//   medium … 設定サイドバー + キャンバス、実行はキャンバスの下に横帯
//   narrow … 縦1列。スマホではサイドバーを置けないので下にまとめる
export type LayoutTier = 'wide' | 'medium' | 'narrow';

const WIDE_MIN = 1100;
const MEDIUM_MIN = 700;

const tierFor = (width: number): LayoutTier => {
    if (width >= WIDE_MIN) return 'wide';
    if (width >= MEDIUM_MIN) return 'medium';
    return 'narrow';
};

export const useLayoutTier = (): LayoutTier => tierFor(useViewportWidth());
