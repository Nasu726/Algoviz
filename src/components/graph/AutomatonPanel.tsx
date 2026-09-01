import React from 'react';
import { PlaybackControls } from '../ui/PlaybackControls';
import { NODE_STROKE, EDGE_COLOR } from '../visualizers/PixiGraphApp';
import { Section, Swatch } from './panelParts';
import type { GraphState } from '../../types/engine';

interface Props {
    state: GraphState | null;
    inputString: string;
    setInputString: (v: string) => void;

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

// 見ながら操作するもの。入力と再生コントロールと判定。
export const AutomatonPanel: React.FC<Props> = ({
    state, inputString, setInputString,
    isPlaying, delay, setDelay,
    onReset, onPlayPause, onStepBack, onStepNext, onRunToEnd,
    horizontal, compact,
}) => {
    const fontSize = compact ? '12px' : '13px';
    const dim = { color: '#90a4ae' };

    const consumed = state?.inputString?.slice(0, state?.inputPos ?? 0) ?? '';
    const rest = state?.inputString?.slice(state?.inputPos ?? 0) ?? '';
    const current = state?.currentState ?? -1;

    const verdict = !state?.finished ? { text: '読み取り中…', color: '#78909c' }
        : state?.stuck ? { text: '遷移が無いので止まりました', color: '#e67e22' }
        : state?.accepted ? { text: '受理', color: '#27ae60' }
        : { text: '拒否', color: '#c0392b' };

    const inputField = (
        <label style={{ display: 'block', fontSize }}>
            入力文字列:
            <input
                type="text" value={inputString} placeholder="例: abba"
                onChange={(e) => setInputString(e.target.value)}
                style={{ width: '100%', marginTop: '4px', fontFamily: 'monospace' }}
            />
        </label>
    );

    // 消費済みと残りを見分けられるように、区切りを挟んで色を分ける
    const tape = (
        <div style={{
            fontFamily: 'monospace', fontSize: compact ? '15px' : '17px',
            wordBreak: 'break-all', lineHeight: 1.6,
        }}>
            {consumed || rest ? (
                <>
                    <span style={{ color: '#90a4ae' }}>{consumed}</span>
                    <span style={{ color: '#e74c3c', fontWeight: 'bold' }}>|</span>
                    <span style={{ color: '#000' }}>{rest}</span>
                </>
            ) : (
                <span style={dim}>（空の入力）</span>
            )}
        </div>
    );

    const progress = (
        <div style={{ fontSize, lineHeight: 1.7, minWidth: 0 }}>
            {tape}
            <div>
                <b>現在の状態</b>:{' '}
                {current >= 0 ? `q${current}` : <span style={dim}>なし</span>}
            </div>
            <div>
                <b>アルファベット</b>:{' '}
                {state?.alphabet
                    ? state.alphabet.split('').join(', ')
                    : <span style={dim}>なし</span>}
            </div>
            <div style={{ marginTop: '6px', fontWeight: 'bold', color: verdict.color }}>
                {verdict.text}
            </div>
        </div>
    );

    const warning = state?.hasNondeterminism && (
        <div style={{
            padding: '6px 8px', borderRadius: '4px', fontSize: '12px',
            backgroundColor: '#fff8e1', border: '1px solid #ffb300', color: '#5d4037',
        }}>
            同じ状態から同じ記号で複数の遷移が出ています。
            決定性オートマトンではないので、先に書かれた遷移だけを使っています。
        </div>
    );

    const legend = (
        <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px 10px' }}>
            <Swatch color={NODE_STROKE[5]} label="初期状態" />
            <Swatch color={NODE_STROKE[2]} label="現在の状態" />
            <Swatch color={NODE_STROKE[3]} label="通った状態" />
            <Swatch color={NODE_STROKE[4]} label="受理して終了" />
            <Swatch color={EDGE_COLOR[2]} label="直前の遷移" isEdge />
            <Swatch color={EDGE_COLOR[3]} label="通った遷移" isEdge />
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
                <div style={{ display: 'flex', flexDirection: 'column', gap: '8px',
                              flex: '1 1 260px', minWidth: 0 }}>
                    {inputField}
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
            <Section title="入力">
                {inputField}
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
