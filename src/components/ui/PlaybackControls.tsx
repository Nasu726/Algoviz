import React from 'react';
import { delayToSlider, sliderToDelay } from './playbackSpeed';

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
    /** 狭い幅。文字を小さくする */
    compact?: boolean;
    /** ページ固有の追加コントロール */
    children?: React.ReactNode;
}

const buttonStyle = (compact?: boolean): React.CSSProperties => ({
    padding: compact ? '6px' : '8px',
    fontSize: compact ? '12px' : undefined,
    fontWeight: 'bold',
    flexShrink: 0,
    whiteSpace: 'nowrap',
    backgroundColor: '#f5f5f5',
    color: '#000000',
});

export const PlaybackControls: React.FC<PlaybackControlsProps> = ({
    isPlaying, ready, delay,
    onLoad, onPlayPause, onStepBack, onStepNext, onRunToEnd, onDelayChange,
    canStepBack, loadLabel = 'ロード', vertical = false, compact, children,
}) => {
    const backEnabled = (canStepBack ?? ready) && !isPlaying;
    const button = buttonStyle(compact);

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
            <div style={{ display: 'flex', flexWrap: 'wrap', justifyContent: 'center', alignItems: 'center', gap: '8px' }}>
                <button onClick={onLoad} style={button}>{loadLabel}</button>
                <button onClick={onPlayPause} disabled={!ready} style={button}>
                    {isPlaying ? '停止' : '実行'}
                </button>
                <button onClick={onStepBack} disabled={!backEnabled} style={button}>戻る</button>
                <button onClick={onStepNext} disabled={!ready || isPlaying} style={button}>進む</button>
                <button onClick={onRunToEnd} disabled={!ready} style={button}>一気に実行</button>
            </div>

            <div style={{ display: 'flex', flexWrap: 'wrap', alignItems: 'center', gap: '10px', minWidth: 0 }}>
                <span style={{
                    fontWeight: 'bold', flexShrink: 0, whiteSpace: 'nowrap',
                    fontSize: compact ? '12px' : undefined,
                }}>
                    実行速度
                    <input
                        type="range" min="0" max="1000"
                        value={delayToSlider(delay)}
                        onChange={(e) => onDelayChange(sliderToDelay(Number(e.target.value)))}
                        style={{ marginLeft: '0.5em', verticalAlign: 'middle', maxWidth: '130px' }}
                    />
                </span>
                {children}
            </div>
        </div>
    );
};
