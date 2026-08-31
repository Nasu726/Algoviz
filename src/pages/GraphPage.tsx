import React, { useEffect, useRef, useState } from 'react';
import { useInterval } from 'react-use';
import { GraphRenderer } from '../components/visualizers/GraphRenderer';
import { VisualizerShell } from '../components/ui/VisualizerShell';
import { SidebarLayout } from '../components/ui/SidebarLayout';
import { GraphSetupPanel } from '../components/graph/GraphSetupPanel';
import { TraversalPanel } from '../components/graph/TraversalPanel';
import { GraphHelp } from '../components/graph/GraphHelp';
import { defaultSettings, engineAlgorithm, isTraversal, VARIANT_TITLE } from '../components/graph/types';
import type { GraphSettings, GraphVariant } from '../components/graph/types';
import { useKeyboardShortcuts } from '../hooks/keyboardShortcut';
import { useLayoutTier } from '../hooks/useLayoutTier';
import { speedUp, speedDown } from '../components/ui/playbackSpeed';
import type { VisualizerEngine, GraphState } from '../types/engine';

interface Props {
    engine: VisualizerEngine;
    onBack: () => void;
    /** このページが見せるものを1つに固定する */
    variant: GraphVariant;
}

export const GraphPage: React.FC<Props> = ({ engine, onBack, variant }) => {
    const tier = useLayoutTier();
    const traversal = isTraversal(variant);

    const [settings, setSettings] = useState<GraphSettings>(() => defaultSettings(variant));
    const update = (patch: Partial<GraphSettings>) => setSettings((s) => ({ ...s, ...patch }));

    // グラフを作り直さずに反映できる設定は個別に持つ
    const [startNode, setStartNode] = useState('0');
    const [goalNode, setGoalNode] = useState('');
    const [automatonStart, setAutomatonStart] = useState('0');
    const [acceptingNodes, setAcceptingNodes] = useState('1, 2');

    const [isPlaying, setIsPlaying] = useState(false);
    const [delay, setDelay] = useState(300);
    const [state, setState] = useState<GraphState | null>(null);
    const [isLoaded, setIsLoaded] = useState(false);
    const [isHelpOpen, setIsHelpOpen] = useState(false);

    // 生成コマンドを組み立てるときに常に最新の設定を読めるようにしておく。
    // useEffect の依存配列に全部並べると、設定を変えるたびにグラフが作り直されてしまう。
    const latest = useRef({ settings, startNode, goalNode, automatonStart, acceptingNodes });
    latest.current = { settings, startNode, goalNode, automatonStart, acceptingNodes };

    // 常に横長。木 / DAG のビジュアライザを作るときは、木は縦長に描くのが普通なので
    // そこで明示的に分ける (AGENTS.md の「先送りにしている判断」を参照)
    const orientation = () => 'horizontal';
    const flags = () => {
        const s = latest.current.settings;
        return {
            skip: s.skipExtension ? 1 : 0,
            dir: s.isDirected ? 1 : 0,
            nodeW: s.useNodeWeights ? 1 : 0,
            selfLoop: s.allowSelfLoop ? 1 : 0,
            sameEdge: s.allowSameEdge ? 1 : 0,
            conn: s.connected ? 1 : 0,
            wt: s.weighted ? 1 : 0,
        };
    };

    // variant 固有の設定を C++ 側へ渡す
    const applyVariantSettings = () => {
        const l = latest.current;
        if (traversal) {
            const goal = l.goalNode.trim() === '' ? -1 : Number(l.goalNode);
            engine.load('setTraversal', `${variant} ${Number(l.startNode) || 0} ${goal}`);
        } else if (variant === 'automaton') {
            engine.load('setStartNode', l.automatonStart);
            engine.load('setAccepting', l.acceptingNodes);
        }
    };

    const readState = () => setState(engine.getState<GraphState>({ withProgress: true }));

    // グラフを作り直したあとは、テキスト欄と頂点数・辺数の欄も実際の値に合わせる。
    // 連結指定で辺が増えた場合などが、入力欄を見れば分かる。
    const readStateAndSync = () => {
        const s = engine.getState<GraphState>({ withText: true, withProgress: true });
        setState(s);
        update({
            inputBuffer: s.graphText ?? latest.current.settings.inputBuffer,
            nodeCount: String(s.nodeCount),
            edgeCount: String(s.edgeCount),
        });
    };

    const generate = (command: string) => {
        setIsPlaying(false);
        engine.load(orientation(), command);
        applyVariantSettings();
        readStateAndSync();
    };

    // ページを開いたとき。variant ごとに C++ 側のクラスが決まる。
    useEffect(() => {
        if (!engine) return;
        setIsPlaying(false);
        engine.setAlgorithm(engineAlgorithm(variant));
        const f = flags();
        const s = latest.current.settings;
        engine.load(orientation(),
            `random ${s.nodeCount} ${s.edgeCount} ${f.skip} ${f.selfLoop} ${f.sameEdge} ${f.dir} ${f.conn} ${f.wt}`);
        applyVariantSettings();
        readStateAndSync();
        setIsLoaded(true);
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [engine, variant]);

    // 始点・終点などはグラフを作り直さずに反映する
    useEffect(() => {
        if (!engine || !isLoaded) return;
        setIsPlaying(false);
        applyVariantSettings();
        readState();
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [startNode, goalNode, automatonStart, acceptingNodes]);

    // === 再生ループ ===
    useInterval(() => {
        if (!isPlaying || !engine) return;
        if (!engine.step()) setIsPlaying(false);
        readState();
    }, isPlaying ? delay : null);

    const handleReset = () => { setIsPlaying(false); engine.load('resetTraversal', ''); readState(); };
    const handleStep = () => { setIsPlaying(false); engine.step(); readState(); };
    const handleStepBack = () => { setIsPlaying(false); engine.stepBack(); readState(); };
    const handleRunToEnd = () => { setIsPlaying(false); engine.runToEnd(); readState(); };

    const handleGenerateRandom = () => {
        const f = flags();
        const s = latest.current.settings;
        generate(`random ${s.nodeCount} ${s.edgeCount} ${f.skip} ${f.selfLoop} ${f.sameEdge} ${f.dir} ${f.conn} ${f.wt}`);
    };
    const handleGenerateComplete = () => {
        const f = flags();
        generate(`complete ${latest.current.settings.nodeCount} ${f.skip} ${f.dir} ${f.wt}`);
    };
    const handleGenerateFromText = () => {
        const f = flags();
        generate(`custom ${f.skip} ${f.dir} ${f.nodeW} ${f.wt}\n${latest.current.settings.inputBuffer}`);
    };

    useKeyboardShortcuts({
        onEsc: !isHelpOpen ? onBack : undefined,
        onHelp: () => setIsHelpOpen(!isHelpOpen),
        onSave: !isHelpOpen ? handleGenerateFromText : undefined,
        onPlayPause: !isHelpOpen && traversal ? () => setIsPlaying(!isPlaying) : undefined,
        onStepNext: !isHelpOpen && traversal ? handleStep : undefined,
        onStepBack: !isHelpOpen && traversal ? handleStepBack : undefined,
        onSpeedUp: () => { if (!isHelpOpen) setDelay(speedUp(delay)); },
        onSpeedDown: () => { if (!isHelpOpen) setDelay(speedDown(delay)); },
    });

    const maxNodes = state?.maxNodes ?? 50;
    const compact = tier === 'narrow';

    const setupPanel = (
        <GraphSetupPanel
            variant={variant}
            settings={settings}
            update={update}
            maxNodes={maxNodes}
            onGenerateRandom={handleGenerateRandom}
            onGenerateComplete={handleGenerateComplete}
            onGenerateFromText={handleGenerateFromText}
            automatonStart={automatonStart}
            setAutomatonStart={setAutomatonStart}
            acceptingNodes={acceptingNodes}
            setAcceptingNodes={setAcceptingNodes}
            compact={compact}
        />
    );

    const traversalPanel = traversal ? (
        <TraversalPanel
            variant={variant}
            state={state}
            maxNodes={maxNodes}
            startNode={startNode} setStartNode={setStartNode}
            goalNode={goalNode} setGoalNode={setGoalNode}
            isPlaying={isPlaying} delay={delay} setDelay={setDelay}
            onReset={handleReset}
            onPlayPause={() => setIsPlaying(!isPlaying)}
            onStepBack={handleStepBack}
            onStepNext={handleStep}
            onRunToEnd={handleRunToEnd}
            horizontal={tier !== 'wide'}
            compact={compact}
        />
    ) : null;

    const canvas = isLoaded ? (
        <GraphRenderer engine={engine} showWeights={settings.showWeights} />
    ) : null;

    return (
        <VisualizerShell
            title={VARIANT_TITLE[variant]}
            compact={compact}
            onBack={onBack}
            backConfirm="ビジュアライザ一覧へ戻りますか？"
            isHelpOpen={isHelpOpen}
            setIsHelpOpen={setIsHelpOpen}
            help={<GraphHelp variant={variant} maxNodes={maxNodes} />}
        >
            <SidebarLayout
                tier={tier}
                setupPanel={setupPanel}
                canvas={canvas}
                controlPanel={traversalPanel}
            />
        </VisualizerShell>
    );
};
