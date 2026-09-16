import { useEffect, useState } from 'react';
import type { ReactNode } from 'react';
import { BrowserRouter, Routes, Route, Navigate, useNavigate } from 'react-router-dom';
import type { VisualizerEngine } from './types/engine';

import { MenuPage } from './pages/Menu';
import { BrainfuckPage } from './pages/BrainfuckPage';
import { GraphPage } from './pages/GraphPage';
import { TreePage } from './pages/TreePage';
import { ArrayPage } from './pages/ArrayPage';
import type { GraphVariant } from './components/graph/types';
import type { TreeVariant } from './components/tree/types';
import type { ArrayVariant } from './components/array/types';

let enginePromise: Promise<VisualizerEngine> | null = null;

// 一覧には WASM は要らない。ビジュアライザを開いたときだけ core.js を読み、
// 以後は同じ VisualizerEngine を使い回す。
const loadEngine = () => {
  if (enginePromise) return enginePromise;

  enginePromise = (async () => {
    if (typeof globalThis.createVisualizerModule !== 'function') {
      await new Promise<void>((resolve, reject) => {
        const existing = document.querySelector<HTMLScriptElement>('script[data-algoviz-wasm]');
        if (existing) {
          if (existing.dataset.loaded === 'true') {
            reject(new Error("'createVisualizerModule' is not defined after core.js loaded."));
            return;
          }
          existing.addEventListener('load', () => resolve(), { once: true });
          existing.addEventListener('error', () => reject(new Error('core.js failed to load.')), { once: true });
          return;
        }

        const script = document.createElement('script');
        script.src = '/wasm/core.js';
        script.async = true;
        script.dataset.algovizWasm = 'true';
        script.addEventListener('load', () => {
          script.dataset.loaded = 'true';
          resolve();
        }, { once: true });
        script.addEventListener('error', () => reject(new Error('core.js failed to load.')), { once: true });
        document.head.appendChild(script);
      });
    }

    const createModule = globalThis.createVisualizerModule;
    if (!createModule) {
      throw new Error("'createVisualizerModule' is not defined.");
    }

    const module = await createModule();
    if (!module.VisualizerEngine) {
      throw new Error('VisualizerEngine class not found in Wasm. Did you rebuild?');
    }

    return new module.VisualizerEngine();
  })().catch((error) => {
    // 一時的な読み込み失敗なら、次にページへ入り直したとき再試行できるようにする。
    enginePromise = null;
    throw error;
  });

  return enginePromise;
};

function EngineGate({ children }: { children: (engine: VisualizerEngine) => ReactNode }) {
  const [engine, setEngine] = useState<VisualizerEngine | null>(null);
  const [loadError, setLoadError] = useState('');

  useEffect(() => {
    let active = true;
    loadEngine().then(
      (loaded) => { if (active) setEngine(loaded); },
      (error) => { if (active) setLoadError(error instanceof Error ? error.message : String(error)); },
    );
    return () => { active = false; };
  }, []);

  if (loadError) {
    return (
      <div style={{ color: '#b71c1c', padding: 20, fontFamily: 'sans-serif' }}>
        <h2>System Error</h2>
        <p>{loadError}</p>
      </div>
    );
  }

  if (!engine) {
    return (
      <div style={{ display: 'flex', justifyContent: 'center', alignItems: 'center', height: '100vh' }}>
        <h3>Wasmエンジンを起動中...</h3>
      </div>
    );
  }

  return <>{children(engine)}</>;
}

function MainMenu() {
  return <MenuPage />;
}

function BrainfuckWrapper({ engine }: { engine: VisualizerEngine }) {
  const navigate = useNavigate();
  return <BrainfuckPage engine={engine} onBack={() => navigate('/')} />;
}

function ArrayWrapper({ engine, variant }: { engine: VisualizerEngine; variant: ArrayVariant }) {
  const navigate = useNavigate();
  return <ArrayPage engine={engine} variant={variant} onBack={() => navigate('/')} />;
}

function TreeWrapper({ engine, variant }: { engine: VisualizerEngine; variant: TreeVariant }) {
  const navigate = useNavigate();
  return <TreePage engine={engine} variant={variant} onBack={() => navigate('/')} />;
}

// グラフ系は1ページ1アルゴリズム。variant がそのままページの中身を決める。
function GraphWrapper({ engine, variant }: { engine: VisualizerEngine; variant: GraphVariant }) {
  const navigate = useNavigate();
  return <GraphPage engine={engine} variant={variant} onBack={() => navigate('/')} />;
}

function App() {
  return (
    <BrowserRouter>
      <Routes>
        {/* 一覧は WASM と独立して即表示する */}
        <Route path="/" element={<MainMenu />} />

        <Route path="/brainfuck" element={
          <EngineGate>{(engine) => <BrainfuckWrapper engine={engine} />}</EngineGate>
        } />

        {/* グラフ探索。1ページ1アルゴリズム */}
        <Route path="/graph/bfs" element={
          <EngineGate>{(engine) => <GraphWrapper engine={engine} variant="bfs" />}</EngineGate>
        } />
        <Route path="/graph/dfs" element={
          <EngineGate>{(engine) => <GraphWrapper engine={engine} variant="dfs" />}</EngineGate>
        } />
        <Route path="/graph/dijkstra" element={
          <EngineGate>{(engine) => <GraphWrapper engine={engine} variant="dijkstra" />}</EngineGate>
        } />
        <Route path="/automaton" element={
          <EngineGate>{(engine) => <GraphWrapper engine={engine} variant="automaton" />}</EngineGate>
        } />

        {/* 木 */}
        <Route path="/tree/bst" element={
          <EngineGate>{(engine) => <TreeWrapper engine={engine} variant="bst" />}</EngineGate>
        } />
        <Route path="/tree/heap" element={
          <EngineGate>{(engine) => <TreeWrapper engine={engine} variant="heap" />}</EngineGate>
        } />
        <Route path="/tree/trie" element={
          <EngineGate>{(engine) => <TreeWrapper engine={engine} variant="trie" />}</EngineGate>
        } />
        <Route path="/tree/huffman" element={
          <EngineGate>{(engine) => <TreeWrapper engine={engine} variant="huffman" />}</EngineGate>
        } />
        <Route path="/tree/avl" element={
          <EngineGate>{(engine) => <TreeWrapper engine={engine} variant="avl" />}</EngineGate>
        } />
        <Route path="/tree/btree" element={
          <EngineGate>{(engine) => <TreeWrapper engine={engine} variant="btree" />}</EngineGate>
        } />

        {/* 配列。1ページ1アルゴリズム */}
        <Route path="/array/bubble" element={
          <EngineGate>{(engine) => <ArrayWrapper engine={engine} variant="bubble" />}</EngineGate>
        } />
        <Route path="/array/selection" element={
          <EngineGate>{(engine) => <ArrayWrapper engine={engine} variant="selection" />}</EngineGate>
        } />
        <Route path="/array/insertion" element={
          <EngineGate>{(engine) => <ArrayWrapper engine={engine} variant="insertion" />}</EngineGate>
        } />
        <Route path="/array/shaker" element={
          <EngineGate>{(engine) => <ArrayWrapper engine={engine} variant="shaker" />}</EngineGate>
        } />
        <Route path="/array/quick" element={
          <EngineGate>{(engine) => <ArrayWrapper engine={engine} variant="quick" />}</EngineGate>
        } />
        <Route path="/array/merge" element={
          <EngineGate>{(engine) => <ArrayWrapper engine={engine} variant="merge" />}</EngineGate>
        } />
        <Route path="/array/linear" element={
          <EngineGate>{(engine) => <ArrayWrapper engine={engine} variant="linear" />}</EngineGate>
        } />
        <Route path="/array/binary" element={
          <EngineGate>{(engine) => <ArrayWrapper engine={engine} variant="binary" />}</EngineGate>
        } />
        <Route path="/array/stack" element={
          <EngineGate>{(engine) => <ArrayWrapper engine={engine} variant="stack" />}</EngineGate>
        } />
        <Route path="/array/queue" element={
          <EngineGate>{(engine) => <ArrayWrapper engine={engine} variant="queue" />}</EngineGate>
        } />
        <Route path="/array/deque" element={
          <EngineGate>{(engine) => <ArrayWrapper engine={engine} variant="deque" />}</EngineGate>
        } />

        {/* 描くだけのページ。メニューには載せないが、レイアウトとパッキングの
            回帰を目視確認する手段としてルートは残す */}
        <Route path="/graph" element={
          <EngineGate>{(engine) => <GraphWrapper engine={engine} variant="plain" />}</EngineGate>
        } />

        {/* 定義の無いパスは真っ白になるので、トップへ送る */}
        <Route path="*" element={<Navigate to="/" replace />} />
      </Routes>
    </BrowserRouter>
  );
}

export default App;
