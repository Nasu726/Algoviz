import { useEffect, useState } from 'react';

// 表示幅を追う。しきい値の判断は呼ぶ側に任せる。
//
// window の resize イベントではなく、実際の箱の幅の変化を見る。
// ページが小さいコンテナに埋め込まれた場合にも正しく追従する。
export const useViewportWidth = (): number => {
    const [width, setWidth] = useState(() => window.innerWidth);

    useEffect(() => {
        const observer = new ResizeObserver((entries) => {
            setWidth(entries[0].contentRect.width);
        });
        observer.observe(document.documentElement);

        // 幅が固まったまま戻らないのは困るので、window の resize も保険として拾う。
        const onResize = () => setWidth(window.innerWidth);
        window.addEventListener('resize', onResize);

        return () => {
            observer.disconnect();
            window.removeEventListener('resize', onResize);
        };
    }, []);

    return width;
};
