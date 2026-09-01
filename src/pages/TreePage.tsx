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
import type { VisualizerEngine, GraphState } from '../types/engine';

interface Props {
    engine: VisualizerEngine;
    onBack: () => void;
}

const TITLE = '二分探索木の構築';

// 木のビジュアライザのページ。GraphPage とは決めることが重ならないので分けている。
// 枠 / 並べ替え / 再生は共通の部品をそのまま使う。
export const TreePage: React.FC<Props> = ({ engine, onBack }) => {
    const tier = useLayoutTier();

    const [valueText, setValueText] = useState('50 30 70 20 40 60 80');
    const [count, setCount] = useState('10');
    const [state, setState] = useState<GraphState | null>(null);
    const [isLoaded, setIsLoaded] = useState(false);
    const [isHelpOpen, setIsHelpOpen] = useState(false);

    // 入力欄の最新値をコマンド組み立て時に読む。依存配列に並べると
    // 入力するたびに木が作り直されてしまう。
    const latest = useRef({ valueText, count });
    latest.current = { valueText, count };

    const readState = () => setState(engine.getState<GraphState>({}));

    const { isPlaying, setIsPlaying, delay, setDelay, toggle, onSpeedUp, onSpeedDown } =
        usePlayback(() => {
            if (!engine.step()) setIsPlaying(false);
            readState();
        });

    const applyValues = () => {
        setIsPlaying(false);
        engine.load('setValues', latest.current.valueText);
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
        engine.setAlgorithm('bst');
        engine.load('setValues', latest.current.valueText);
        readState();
        setIsLoaded(true);
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [engine]);

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
    const maxValues = state?.maxValues ?? 50;

    return (
        <VisualizerShell
            title={TITLE}
            compact={compact}
            onBack={onBack}
            backConfirm="ビジュアライザ一覧へ戻りますか？"
            isHelpOpen={isHelpOpen}
            setIsHelpOpen={setIsHelpOpen}
            help={<TreeHelp maxValues={maxValues} />}
        >
            <SidebarLayout
                tier={tier}
                setupPanel={
                    <TreeSetupPanel
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
                canvas={isLoaded ? <GraphRenderer engine={engine} showWeights={false} /> : null}
                controlPanel={
                    <TreePanel
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
