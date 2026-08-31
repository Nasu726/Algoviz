import React from 'react';

// 実行速度スライダーの目盛りと delay(ms) の対応。
// 左端で 1秒/ステップ、右端で 0ms になるよう二乗で効かせている。
export const delayToSlider = (delay: number) => 1000 - Math.sqrt(1000 * delay);
export const sliderToDelay = (x: number) => ((x - 1000) * (x - 1000)) / 1000;

// キーボードショートカット (Ctrl + ↑ / ↓) 用。スライダーを 100 目盛り分動かすのと同じ。
export const speedUp = (delay: number) =>
    delay >= 10 ? Math.max(0, ((-Math.sqrt(1000 * delay) + 100) ** 2) / 1000) : 0;
export const speedDown = (delay: number) =>
    Math.max(0, ((-Math.sqrt(1000 * delay) - 100) ** 2) / 1000);

interface PlaybackControlsProps {
    isPlaying: boolean;
    /** ロード済みで操作を受け付けられるか */
    ready: boolean;
    delay: number;
    onLoad: () => void;
    onPlayPause: () => void;
    onStepBack: () => void;
    onStepNext: () => void;
    onRunToEnd: () => void;
    onDelayChange: (delay: number) => void;
    /** 戻る が押せるか。省略時は ready かつ停止中 */
    canStepBack?: boolean;
    loadLabel?: string;
    /** 縦に積む (サイドバー用) */
    vertical?: boolean;
    /** ページ固有の追加コントロール */
    children?: React.ReactNode;
}

const buttonStyle: React.CSSProperties = {
    padding: '8px',
    fontWeight: 'bold',
    flexShrink: 0,
    whiteSpace: 'nowrap',
    backgroundColor: '#f5f5f5',
    color: '#000000',
};

export const PlaybackControls: React.FC<PlaybackControlsProps> = ({
    isPlaying, ready, delay,
    onLoad, onPlayPause, onStepBack, onStepNext, onRunToEnd, onDelayChange,
    canStepBack, loadLabel = 'ロード', vertical = false, children,
}) => {
    const backEnabled = (canStepBack ?? ready) && !isPlaying;

    return (
        <div style={{
            display: 'flex',
            flexDirection: vertical ? 'column' : 'row',
            flexWrap: 'wrap',
            justifyContent: 'center',
            alignItems: 'center',
            gap: '10px',
            padding: vertical ? '0' : '0 20px',
        }}>
            <div style={{ display: 'flex', flexWrap: 'wrap', justifyContent: 'center', alignItems: 'center', gap: '10px' }}>
                <button onClick={onLoad} style={buttonStyle}>{loadLabel}</button>
                <button onClick={onPlayPause} disabled={!ready} style={buttonStyle}>
                    {isPlaying ? '停止' : '実行'}
                </button>
                <button onClick={onStepBack} disabled={!backEnabled} style={buttonStyle}>戻る</button>
                <button onClick={onStepNext} disabled={!ready || isPlaying} style={buttonStyle}>進む</button>
            </div>

            <div style={{ display: 'flex', flexWrap: 'wrap', alignItems: 'center', gap: '10px', flexShrink: 0 }}>
                <span style={{ fontWeight: 'bold', flexShrink: 0, whiteSpace: 'nowrap' }}>
                    実行速度
                    <input
                        type="range" min="0" max="1000"
                        value={delayToSlider(delay)}
                        onChange={(e) => onDelayChange(sliderToDelay(Number(e.target.value)))}
                        style={{ marginLeft: '0.5em', verticalAlign: 'middle' }}
                    />
                </span>
                <button
                    onClick={onRunToEnd}
                    disabled={!ready}
                    style={{ ...buttonStyle, backgroundColor: '#fcfcfc', color: '#ff0000' }}
                >
                    一気に実行
                </button>
                {children}
            </div>
        </div>
    );
};
