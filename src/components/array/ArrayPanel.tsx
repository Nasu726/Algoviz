import React from 'react';
import { PlaybackControls } from '../ui/PlaybackControls';
import { NODE_STROKE } from '../visualizers/PixiGraphApp';
import { Section, Swatch } from '../graph/panelParts';
import type { ArrayVariant } from './types';
import type { GraphState } from '../../types/engine';

interface Props {
    variant: ArrayVariant;
    state: GraphState | null;

    isPlaying: boolean;
    delay: number;
    setDelay: (v: number) => void;
    onReset: () => void;
    onPlayPause: () => void;
    onStepBack: () => void;
    onStepNext: () => void;
    onRunToEnd: () => void;

    /** 横帯として置くとき。中身を横並びにする */
    horizontal?: boolean;
    compact?: boolean;
}

// 見ながら操作するもの。再生コントロールと進行状況。
export const ArrayPanel: React.FC<Props> = ({
    state, isPlaying, delay, setDelay,
    onReset, onPlayPause, onStepBack, onStepNext, onRunToEnd,
    horizontal, compact,
}) => {
    const fontSize = compact ? '12px' : '13px';

    const total = (state?.values ?? []).length;
    const sortedFrom = state?.sortedFrom ?? total;
    const settled = state?.finished ? total : total - sortedFrom;

    const statusText = state?.finished ? '並び終えました'
        : state?.swapped ? '大小が逆だったので入れ替えました'
        : '隣どうしを比べています';

    const progress = (
        <div style={{ fontSize, lineHeight: 1.7, minWidth: 0 }}>
            <div>
                <b>位置が確定した個数</b>: {settled}
                <span style={{ color: '#90a4ae' }}> / {total}</span>
            </div>
            <div style={{ marginTop: '6px', fontWeight: 'bold',
                          color: state?.finished ? '#27ae60' : '#78909c' }}>
                {statusText}
            </div>
        </div>
    );

    const legend = (
        <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px 10px' }}>
            <Swatch color={NODE_STROKE[2]} label="比べている2つ" />
            <Swatch color={NODE_STROKE[4]} label="入れ替えた2つ" />
            <Swatch color={NODE_STROKE[3]} label="位置が確定した値" />
        </div>
    );

    const controls = (
        <PlaybackControls
            isPlaying={isPlaying}
            ready={!!state}
            canStepBack={!!state?.canStepBack}
            delay={delay}
            loadLabel="最初から"
            onLoad={onReset}
            onPlayPause={onPlayPause}
            onStepBack={onStepBack}
            onStepNext={onStepNext}
            onRunToEnd={onRunToEnd}
            onDelayChange={setDelay}
            vertical={!horizontal}
            compact={compact}
        />
    );

    if (horizontal) {
        return (
            <div style={{
                display: 'flex', flexWrap: 'wrap', alignItems: 'flex-start',
                gap: '16px 24px', padding: '10px 15px',
                borderTop: '1px solid #ddd', background: '#f8f9fa',
            }}>
                <div style={{ flex: '1 1 260px', minWidth: 0 }}>{controls}</div>
                <div style={{ flex: '1 1 200px', minWidth: 0 }}>{progress}</div>
                <div style={{ flex: '1 1 200px', minWidth: 0 }}>{legend}</div>
            </div>
        );
    }

    return (
        <div style={{ display: 'flex', flexDirection: 'column', gap: '14px' }}>
            <Section title="実行">{controls}</Section>
            <Section title="実行状態">
                {progress}
                {legend}
            </Section>
        </div>
    );
};
