import React from 'react';
import { PlaybackControls } from '../ui/PlaybackControls';
import { NODE_STROKE, EDGE_COLOR } from '../visualizers/PixiGraphApp';
import { Section, Swatch, NumberInput } from './panelParts';
import type { GraphState } from '../../types/engine';

interface Props {
    variant: 'bfs' | 'dfs' | 'dijkstra';
    state: GraphState | null;
    maxNodes: number;

    startNode: string;
    setStartNode: (v: string) => void;
    goalNode: string;
    setGoalNode: (v: string) => void;

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

const fmt = (v: number) => (Number.isFinite(v) ? String(v) : '∞');

// 見ながら操作するもの。再生コントロールと実行状態。
export const TraversalPanel: React.FC<Props> = ({
    variant, state, maxNodes,
    startNode, setStartNode, goalNode, setGoalNode,
    isPlaying, delay, setDelay,
    onReset, onPlayPause, onStepBack, onStepNext, onRunToEnd,
    horizontal, compact,
}) => {
    const isDijkstra = variant === 'dijkstra';
    const frontierLabel = variant === 'bfs' ? 'キュー'
        : variant === 'dfs' ? 'スタック'
        : '優先度付きキュー';
    const orderLabel = isDijkstra ? '確定順' : '訪問順';

    const frontier = state?.frontier ?? [];
    const visitOrder = state?.visitOrder ?? [];
    const path = state?.path ?? [];
    const distances = state?.distances ?? [];
    const current = state?.current ?? -1;
    const total = state?.nodeCount ?? 0;

    const dim = { color: '#90a4ae' };
    const fontSize = compact ? '12px' : '13px';

    const statusText = !state?.finished ? '探索中…'
        : state?.found ? '終点に到達しました'
        : goalNode.trim() === '' ? '到達できる範囲を調べ終えました'
        : '終点には到達できませんでした';

    const progress = (
        <div style={{ fontSize, lineHeight: 1.7, minWidth: 0 }}>
            <div>
                <b>{frontierLabel}</b>:{' '}
                {frontier.length ? frontier.join(' → ') : <span style={dim}>空</span>}
            </div>
            <div>
                <b>処理中</b>: {current >= 0 ? current : <span style={dim}>なし</span>}
            </div>
            <div>
                <b>{orderLabel}</b>:{' '}
                {visitOrder.length ? visitOrder.join(', ') : <span style={dim}>なし</span>}
                <span style={dim}> ({visitOrder.length} / {total})</span>
            </div>
            {isDijkstra && (
                <div style={{ wordBreak: 'break-all' }}>
                    <b>距離</b>:{' '}
                    {distances.length
                        ? distances.map((d, i) => `${i}:${fmt(d)}`).join('  ')
                        : <span style={dim}>なし</span>}
                </div>
            )}
            <div>
                <b>経路</b>: {path.length ? path.join(' → ') : <span style={dim}>未発見</span>}
                {isDijkstra && path.length > 0 && state?.goalDistance !== undefined && (
                    <span style={dim}> (長さ {fmt(state.goalDistance)})</span>
                )}
            </div>
            <div style={{ marginTop: '6px', fontWeight: 'bold', color: state?.found ? '#27ae60' : '#78909c' }}>
                {statusText}
            </div>
        </div>
    );

    const legend = (
        <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px 10px' }}>
            <Swatch color={NODE_STROKE[0]} label="未訪問" />
            <Swatch color={NODE_STROKE[1]} label={frontierLabel + 'の中'} />
            <Swatch color={NODE_STROKE[2]} label="処理中" />
            <Swatch color={NODE_STROKE[3]} label={isDijkstra ? '確定済み' : '訪問済み'} />
            <Swatch color={NODE_STROKE[4]} label="経路上" />
            <Swatch color={EDGE_COLOR[1]} label="探索木の辺" isEdge />
            <Swatch color={EDGE_COLOR[3]} label="調べ済みの辺" isEdge />
            <Swatch color={EDGE_COLOR[4]} label="経路の辺" isEdge />
        </div>
    );

    const warning = isDijkstra && state?.hasNegativeEdge && (
        <div style={{
            padding: '6px 8px', borderRadius: '4px', fontSize: '12px',
            backgroundColor: '#fff8e1', border: '1px solid #ffb300', color: '#5d4037',
        }}>
            負の重みの辺があります。ダイクストラ法は非負の重みを前提にしているので、
            求まる距離が最短とは限りません。
        </div>
    );

    const endpoints = (
        <div style={{ fontSize, display: 'flex', flexWrap: 'wrap', gap: '10px' }}>
            <span>始点 s: <NumberInput value={startNode} max={maxNodes - 1} onChange={setStartNode} /></span>
            <span>終点 t: <NumberInput value={goalNode} placeholder="なし" max={maxNodes - 1} onChange={setGoalNode} /></span>
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
        />
    );

    // 横帯として置くときは、再生 / 進行状況 / 凡例を横に並べる
    if (horizontal) {
        return (
            <div style={{
                display: 'flex', flexWrap: 'wrap', alignItems: 'flex-start',
                gap: '16px 24px', padding: '10px 15px',
                borderTop: '1px solid #ddd', background: '#f8f9fa',
            }}>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '8px',
                              flex: '1 1 260px', minWidth: 0 }}>
                    {endpoints}
                    {controls}
                </div>
                <div style={{ flex: '1 1 260px', minWidth: 0 }}>{progress}</div>
                <div style={{ flex: '1 1 200px', minWidth: 0, display: 'flex', flexDirection: 'column', gap: '8px' }}>
                    {warning}
                    {legend}
                </div>
            </div>
        );
    }

    return (
        <div style={{ display: 'flex', flexDirection: 'column', gap: '14px' }}>
            <Section title="探索">
                {endpoints}
                {controls}
            </Section>
            <Section title="実行状態">
                {progress}
                {warning}
                {legend}
            </Section>
        </div>
    );
};
