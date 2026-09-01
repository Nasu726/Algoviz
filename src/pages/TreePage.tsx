import React, { useEffect, useRef, useState } from 'react';
import { GraphRenderer } from '../components/visualizers/GraphRenderer';
import { VisualizerShell } from '../components/ui/VisualizerShell';
import { SidebarLayout } from '../components/ui/SidebarLayout';
import { TreeSetupPanel } from '../components/tree/TreeSetupPanel';
import { TreePanel } from '../components/tree/TreePanel';
import { TreeHelp } from '../components/tree/TreeHelp';
import { useKeyboardShortcuts } from '../hooks/keyboardShortcut';
import { useLayoutTier } from '../hooks/useLayoutTier';
import { usePlayback } from '../hooks/usePlayback';
import { TREE_TITLE, treeAlgorithm, defaultValues, usesWords, usesText } from '../components/tree/types';
import type { TreeVariant } from '../components/tree/types';
import type { VisualizerEngine, GraphState } from '../types/engine';

interface Props {
    engine: VisualizerEngine;
    onBack: () => void;
    /** このページが見せるものを1つに固定する */
    variant: TreeVariant;
}

// 木のビジュアライザのページ。GraphPage とは決めることが重ならないので分けている。
// 枠 / 並べ替え / 再生は共通の部品をそのまま使う。
//
// variant ごとの違いはパネルの中に閉じ込め、ここは題とコマンド名だけを変える。
export const TreePage: React.FC<Props> = ({ engine, onBack, variant }) => {
    const tier = useLayoutTier();

    const [valueText, setValueText] = useState(defaultValues[variant]);
    const [maxHeap, setMaxHeap] = useState(true);
    const [order, setOrder] = useState(4);
    const [count, setCount] = useState('10');
    const [state, setState] = useState<GraphState | null>(null);
    const [isLoaded, setIsLoaded] = useState(false);
    const [isHelpOpen, setIsHelpOpen] = useState(false);

    // 入力欄の最新値をコマンド組み立て時に読む。依存配列に並べると
    // 入力するたびに木が作り直されてしまう。
    const latest = useRef({ valueText, count });
    latest.current = { valueText, count };

    const readState = () => setState(engine.getState<GraphState>({}));

    // 何を入れるかは variant で決まる
    const loadCommand = usesText(variant) ? 'setText'
        : usesWords(variant) ? 'setWords'
        : 'setValues';

    const { isPlaying, setIsPlaying, delay, setDelay, toggle, onSpeedUp, onSpeedDown } =
        usePlayback(() => {
            if (!engine.step()) setIsPlaying(false);
            readState();
        });

    const applyValues = () => {
        setIsPlaying(false);
        engine.load(loadCommand, latest.current.valueText);
        readState();
    };

    const generateRandom = () => {
        setIsPlaying(false);
        engine.load('genRandom', latest.current.count);
        // 生成された値を入力欄にも反映する
        const s = engine.getState<GraphState>({});
        setState(s);
        setValueText((s.values ?? []).join(' '));
    };

    useEffect(() => {
        if (!engine) return;
        setIsPlaying(false);
        engine.setAlgorithm(treeAlgorithm(variant));
        engine.load(loadCommand, latest.current.valueText);
        readState();
        setIsLoaded(true);
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [engine, variant]);

    // 次数が変われば出来上がる木も変わるので、C++ 側で作り直す
    const changeOrder = (v: number) => {
        setIsPlaying(false);
        setOrder(v);
        engine.load('setOrder', String(v));
        engine.load('setValues', latest.current.valueText);
        readState();
    };

    // 向きが変われば出来上がる木も変わるので、C++ 側で作り直す
    const changeMaxHeap = (v: boolean) => {
        setIsPlaying(false);
        setMaxHeap(v);
        engine.load('setMaxHeap', v ? '1' : '0');
        engine.load('setValues', latest.current.valueText);
        readState();
    };

    const handleReset = () => { setIsPlaying(false); engine.load('resetRun', ''); readState(); };
    const handleStep = () => { setIsPlaying(false); engine.step(); readState(); };
    const handleStepBack = () => { setIsPlaying(false); engine.stepBack(); readState(); };
    const handleRunToEnd = () => { setIsPlaying(false); engine.runToEnd(); readState(); };

    useKeyboardShortcuts({
        onEsc: !isHelpOpen ? onBack : undefined,
        onHelp: () => setIsHelpOpen(!isHelpOpen),
        onSave: !isHelpOpen ? applyValues : undefined,
        onPlayPause: !isHelpOpen ? toggle : undefined,
        onStepNext: !isHelpOpen ? handleStep : undefined,
        onStepBack: !isHelpOpen ? handleStepBack : undefined,
        onSpeedUp: () => { if (!isHelpOpen) onSpeedUp(); },
        onSpeedDown: () => { if (!isHelpOpen) onSpeedDown(); },
    });

    const compact = tier === 'narrow';
    const maxValues = usesText(variant) ? (state?.maxLeaves ?? 25)
        : usesWords(variant) ? (state?.maxWords ?? 12)
        : (state?.maxValues ?? 50);

    return (
        <VisualizerShell
            title={TREE_TITLE[variant]}
            compact={compact}
            onBack={onBack}
            backConfirm="ビジュアライザ一覧へ戻りますか？"
            isHelpOpen={isHelpOpen}
            setIsHelpOpen={setIsHelpOpen}
            help={<TreeHelp variant={variant} maxValues={maxValues} />}
        >
            <SidebarLayout
                tier={tier}
                setupPanel={
                    <TreeSetupPanel
                        variant={variant}
                        maxHeap={maxHeap}
                        setMaxHeap={changeMaxHeap}
                        order={order}
                        setOrder={changeOrder}
                        valueText={valueText}
                        setValueText={setValueText}
                        count={count}
                        setCount={setCount}
                        maxValues={maxValues}
                        onApply={applyValues}
                        onGenerateRandom={generateRandom}
                        compact={compact}
                    />
                }
                canvas={isLoaded ? <GraphRenderer engine={engine} showWeights={usesText(variant) || variant === 'avl'} /> : null}
                controlPanel={
                    <TreePanel
                        variant={variant}
                        state={state}
                        isPlaying={isPlaying}
                        delay={delay}
                        setDelay={setDelay}
                        onReset={handleReset}
                        onPlayPause={toggle}
                        onStepBack={handleStepBack}
                        onStepNext={handleStep}
                        onRunToEnd={handleRunToEnd}
                        horizontal={tier !== 'wide'}
                        compact={compact}
                    />
                }
            />
        </VisualizerShell>
    );
};
