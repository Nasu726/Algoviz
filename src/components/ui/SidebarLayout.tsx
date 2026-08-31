import React from 'react';
import type { LayoutTier } from '../../hooks/useLayoutTier';

interface Props {
    tier: LayoutTier;
    /** 実行前に決める設定。左サイドバー */
    setupPanel: React.ReactNode;
    /** 中央の描画領域 */
    canvas: React.ReactNode;
    /** 実行状態と再生コントロール。広いときだけ右サイドバーになる */
    controlPanel?: React.ReactNode;
}

const sidebarStyle: React.CSSProperties = {
    flexShrink: 0, overflowY: 'auto', padding: '15px', background: '#f8f9fa',
};

// 設定 / 描画 / 実行 の3枚を画面幅に応じて並べ替える。
//
// 3つの配置で DOM の構造を変えないのが肝心。
// キャンバスの位置が変わると描画コンポーネントが再マウントされ、
// PixiJS のアプリが作り直されてカメラ位置も失われる。
// 並びは flexDirection と order だけで切り替える。
export const SidebarLayout: React.FC<Props> = ({ tier, setupPanel, canvas, controlPanel }) => {
    const narrow = tier === 'narrow';
    const wide = tier === 'wide';

    return (
        <div style={{
            display: 'flex',
            flexDirection: narrow ? 'column' : 'row',
            flex: 1, minHeight: 0,
            overflowY: narrow ? 'auto' : 'hidden',
        }}>
            {/* 設定。狭いときは一番下へ回す */}
            <div style={{
                ...sidebarStyle,
                order: narrow ? 2 : 0,
                width: narrow ? 'auto' : (wide ? '280px' : '260px'),
                borderRight: narrow ? 'none' : '1px solid #ddd',
                overflowY: narrow ? 'visible' : 'auto',
            }}>
                {setupPanel}
            </div>

            {/* キャンバスと、広くないときの実行帯 */}
            <div style={{
                order: 1, flex: narrow ? 'none' : 1,
                display: 'flex', flexDirection: 'column', minWidth: 0,
            }}>
                <div style={{
                    flex: narrow ? 'none' : 1,
                    height: narrow ? '45vh' : 'auto',
                    display: 'flex', minHeight: 0, padding: narrow ? '10px' : '15px',
                }}>
                    {canvas}
                </div>
                {!wide && controlPanel}
            </div>

            {/* 広いときだけ右サイドバー */}
            {wide && controlPanel && (
                <div style={{ ...sidebarStyle, order: 2, width: '280px', borderLeft: '1px solid #ddd' }}>
                    {controlPanel}
                </div>
            )}
        </div>
    );
};
