import { useEffect, useState } from 'react';

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

export const useLayoutTier = (): LayoutTier => {
    const [tier, setTier] = useState<LayoutTier>(() => tierFor(window.innerWidth));

    useEffect(() => {
        // window の resize イベントではなく、実際の箱の幅の変化を見る。
        // ページが小さいコンテナに埋め込まれた場合にも正しく追従する。
        const observer = new ResizeObserver((entries) => {
            setTier(tierFor(entries[0].contentRect.width));
        });
        observer.observe(document.documentElement);

        // レイアウトが固まったまま戻らないのは困るので、
        // window の resize も保険として拾っておく。
        const onResize = () => setTier(tierFor(window.innerWidth));
        window.addEventListener('resize', onResize);

        return () => {
            observer.disconnect();
            window.removeEventListener('resize', onResize);
        };
    }, []);

    return tier;
};
