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

const colorName = (side: number | undefined) => (side === 0 ? '青' : '橙');

// 二部グラフ判定の再生コントロールと実行状態。
export const BipartitePanel: React.FC<Props> = ({
    state, isPlaying, delay, setDelay,
    onReset, onPlayPause, onStepBack, onStepNext, onRunToEnd,
    horizontal, compact,
}) => {
    const edges = state?.edges;
    const sides = state?.sides ?? [];
    const queue = state?.frontier ?? [];
    const current = state?.current ?? -1;
    const u = state?.actionVertex ?? -1;

    const dim = { color: '#90a4ae' };
    const fontSize = compact ? '12px' : '13px';

    const endpoints = (e: number) => edges
        ? { a: edges[e * EDGE_STRIDE], b: edges[e * EDGE_STRIDE + 1] }
        : { a: 0, b: 0 };
    // この手で見た辺の、u と反対側の端
    const neighbor = (() => {
        const e = state?.lookingEdge ?? -1;
        if (e < 0) return -1;
        const { a, b } = endpoints(e);
        return a === u ? b : a;
    })();

    const statusText = (() => {
        if (state?.finished && state.bipartite) return '全部の頂点を塗れました。二部グラフです';
        if (state?.finished) {
            const { a, b } = endpoints(state.conflictEdge ?? 0);
            return `辺 ${a}–${b} の両端がどちらも${colorName(sides[a])}。二部グラフではありません`;
        }
        switch (state?.action) {
            case 'start': return `頂点 ${u} を青で塗り、ここから始めます`;
            case 'paint': return `${u} の隣 ${neighbor} を${colorName(sides[neighbor])}で塗りました`;
            case 'fine':  return `${u} の隣 ${neighbor} は既に${colorName(sides[neighbor])}。問題ありません`;
            case 'done':  return current < 0
                ? `${u} の隣を見終えました。次は塗られていない頂点から始めます`
                : `${u} の隣を見終えました`;
            default:      return '塗られていない最小の番号から始めます';
        }
    })();

    const statusColor = !state?.finished ? '#78909c' : state.bipartite ? '#27ae60' : '#c0392b';

    const progress = (
        <div style={{ fontSize, lineHeight: 1.7, minWidth: 0 }}>
            <div>
                <b>キュー</b>: {queue.length === 0 ? <span style={dim}>空</span> : queue.join(', ')}
            </div>
            <div>
                <b>処理中</b>: {current >= 0 ? current : <span style={dim}>なし</span>}
            </div>
            <div>
                <b>塗った頂点</b>: {state?.coloredCount ?? 0} / {state?.nodeCount ?? 0}
                <span style={dim}>{'　'}成分: {state?.componentCount ?? 0}</span>
            </div>
            <div style={{ marginTop: '6px', fontWeight: 'bold', color: statusColor }}>
                {statusText}
            </div>
        </div>
    );

    const legend = (
        <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px 10px' }}>
            <Swatch color={NODE_STROKE[5]} label="青の側" />
            <Swatch color={NODE_STROKE[1]} label="橙の側" />
            <Swatch color={NODE_STROKE[0]} label="まだ塗っていない" />
            <Swatch color={EDGE_COLOR[2]} label="今見ている辺 (根元が処理中)" isEdge />
            <Swatch color={EDGE_COLOR[1]} label="色を伝えた辺" isEdge />
            <Swatch color={EDGE_COLOR[3]} label="確かめた辺" isEdge />
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
