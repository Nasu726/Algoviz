import React from 'react';
import { PlaybackControls } from '../ui/PlaybackControls';
import { NODE_STROKE, EDGE_COLOR } from '../visualizers/PixiGraphApp';
import { Section, Swatch } from '../graph/panelParts';
import type { TreeVariant } from './types';
import type { GraphState } from '../../types/engine';

interface Props {
    variant: TreeVariant;
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
export const TreePanel: React.FC<Props> = ({
    variant, state, isPlaying, delay, setDelay,
    onReset, onPlayPause, onStepBack, onStepNext, onRunToEnd,
    horizontal, compact,
}) => {
    const fontSize = compact ? '12px' : '13px';
    const dim = { color: '#90a4ae' };

    const values = state?.values ?? [];
    const pending = state?.pending ?? 0;
    const cursor = state?.cursor ?? -1;
    const inserted = state?.insertedCount ?? 0;

    const heap = variant === 'heap';
    const statusText = state?.finished ? 'すべて挿入し終えました'
        : cursor >= 0 ? (heap ? '親と比べながら上げています' : '比べながら降りています')
        : (heap ? '次の値を末尾に置きます' : '次の値を根から入れます');

    // 挿入済みと、これから挿入する値を色で分ける
    const queue = (
        <div style={{ fontFamily: 'monospace', fontSize: compact ? '14px' : '16px',
                      wordBreak: 'break-all', lineHeight: 1.7 }}>
            {values.length === 0 ? <span style={dim}>（値がありません）</span> : values.map((v, i) => (
                <span key={i} style={{
                    marginRight: '8px',
                    color: i < pending ? '#90a4ae' : i === pending ? '#e74c3c' : '#000',
                    fontWeight: i === pending ? 'bold' : 'normal',
                }}>
                    {v}
                </span>
            ))}
        </div>
    );

    const progress = (
        <div style={{ fontSize, lineHeight: 1.7, minWidth: 0 }}>
            {queue}
            <div>
                <b>今挿入している値</b>:{' '}
                {pending < values.length ? values[pending] : <span style={dim}>なし</span>}
            </div>
            <div>
                <b>{heap ? '上げている位置' : '比べている節点'}</b>:{' '}
                {cursor >= 0 ? '光っている節点' : <span style={dim}>なし</span>}
            </div>
            <div>
                <b>節点の数</b>: {inserted}
                <span style={dim}> / {values.length}</span>
            </div>
            {!heap && state?.duplicate && (
                <div style={{ color: '#e67e22' }}>同じ値が既にあったので入れませんでした</div>
            )}
            <div style={{ marginTop: '6px', fontWeight: 'bold',
                          color: state?.finished ? '#27ae60' : '#78909c' }}>
                {statusText}
            </div>
        </div>
    );

    const legend = (
        <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px 10px' }}>
            {heap ? (
                <>
                    <Swatch color={NODE_STROKE[2]} label="上げている位置" />
                    <Swatch color={NODE_STROKE[1]} label="比べている親" />
                    <Swatch color={NODE_STROKE[4]} label="直前に入れ替えた位置" />
                    <Swatch color={EDGE_COLOR[2]} label="今比べている枝" isEdge />
                </>
            ) : (
                <>
                    <Swatch color={NODE_STROKE[2]} label="比べている節点" />
                    <Swatch color={NODE_STROKE[3]} label="通った節点" />
                    <Swatch color={NODE_STROKE[4]} label="今つないだ節点" />
                    <Swatch color={EDGE_COLOR[2]} label="直前に降りた枝" isEdge />
                    <Swatch color={EDGE_COLOR[3]} label="通った枝" isEdge />
                </>
            )}
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
                <div style={{ flex: '1 1 260px', minWidth: 0 }}>{progress}</div>
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
