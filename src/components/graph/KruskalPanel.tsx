import React from 'react';
import { PlaybackControls } from '../ui/PlaybackControls';
import { NODE_STROKE, EDGE_COLOR } from '../visualizers/PixiGraphApp';
import { Section, Swatch } from './panelParts';
import type { GraphState } from '../../types/engine';

interface Props {
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

const EDGE_STRIDE = 4; // [from, to, weight, colorId]

// クラスカル法の再生コントロールと実行状態。
// 軽い順の辺の一覧が主役で、▶ が上から下へ動いていく。
export const KruskalPanel: React.FC<Props> = ({
    state, isPlaying, delay, setDelay,
    onReset, onPlayPause, onStepBack, onStepNext, onRunToEnd,
    horizontal, compact,
}) => {
    const edges = state?.edges;
    const order = state?.edgeOrder ?? [];
    const status = state?.edgeStatus ?? [];
    const current = state?.current ?? -1;
    const nodeCount = state?.nodeCount ?? 0;

    const dim = { color: '#90a4ae' };
    const fontSize = compact ? '12px' : '13px';

    const endpoints = (e: number) => edges
        ? { u: edges[e * EDGE_STRIDE], v: edges[e * EDGE_STRIDE + 1], w: edges[e * EDGE_STRIDE + 2] }
        : { u: 0, v: 0, w: 0 };
    const label = (e: number) => { const { u, v, w } = endpoints(e); return `${u}–${v} (${w})`; };

    const statusText = (() => {
        if (state?.finished) return '全部の辺を見終えました';
        if (current < 0) return '軽い辺から順に見ていきます';
        const { u, v, w } = endpoints(current);
        if (state?.looking) return `辺 ${u}–${v} (重み ${w}) を見ています`;
        return state?.decision === 'accept'
            ? `${u} と ${v} は別の木なので採用`
            : `${u} と ${v} は既につながっているので却下`;
    })();

    const progress = (
        <div style={{ fontSize, lineHeight: 1.7, minWidth: 0 }}>
            <div style={{ display: 'flex', flexWrap: 'wrap', gap: '2px 10px' }}>
                <b>軽い順</b>:
                {order.length === 0 && <span style={dim}>辺がありません</span>}
                {order.map((e) => {
                    const s = status[e] ?? 0;
                    const mark = e === current && state?.looking ? '▶' : s === 1 ? '✓' : s === 2 ? '✗' : '・';
                    const color = s === 1 ? '#27ae60' : s === 2 ? '#90a4ae' : e === current ? '#e67e22' : '#37474f';
                    return (
                        <span key={e} style={{ color, whiteSpace: 'nowrap',
                                                fontWeight: e === current ? 'bold' : 'normal' }}>
                            {mark}{label(e)}
                        </span>
                    );
                })}
            </div>
            <div>
                <b>採用</b>: {state?.treeEdges ?? 0} / {Math.max(nodeCount - 1, 0)}
                <span style={dim}>{'　'}合計: {state?.treeWeight ?? 0}</span>
            </div>
            <div style={{ marginTop: '6px', fontWeight: 'bold', color: state?.finished ? '#27ae60' : '#78909c' }}>
                {statusText}
            </div>
        </div>
    );

    const legend = (
        <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px 10px' }}>
            <Swatch color={EDGE_COLOR[2]} label="今見ている辺" isEdge />
            <Swatch color={EDGE_COLOR[1]} label="採用した辺" isEdge />
            <Swatch color={EDGE_COLOR[3]} label="却下した辺" isEdge />
            <Swatch color={NODE_STROKE[2]} label="見ている辺の両端" />
            <Swatch color={NODE_STROKE[4]} label="採用した辺の両端" />
            <Swatch color={NODE_STROKE[3]} label="却下した辺の両端" />
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
