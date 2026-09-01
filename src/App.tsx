import { useEffect, useState, useRef } from 'react';
import { BrowserRouter, Routes, Route, Navigate, useNavigate } from 'react-router-dom';
import type { VisualizerEngine, CreateVisualizerModule } from './types/engine';

import { MenuPage } from './pages/Menu';
import { BrainfuckPage } from './pages/BrainfuckPage';
import { GraphPage } from './pages/GraphPage';
import { TreePage } from './pages/TreePage';
import type { GraphVariant } from './components/graph/types';
import type { TreeVariant } from './components/tree/types';

function MainMenu() {
  return (
    <MenuPage/>
  );
}

function BrainfuckWrapper({ engine }: { engine: VisualizerEngine }) {
  const navigate = useNavigate();
  return (
    <BrainfuckPage 
      engine={engine} 
      onBack={() => navigate('/')} // ★ '/' (トップ) へ遷移
    />
  );
}

function TreeWrapper({ engine, variant }: { engine: VisualizerEngine; variant: TreeVariant }) {
  const navigate = useNavigate();
  return <TreePage engine={engine} variant={variant} onBack={() => navigate('/')} />;
}

// グラフ系は1ページ1アルゴリズム。variant がそのままページの中身を決める。
function GraphWrapper({ engine, variant }: { engine: VisualizerEngine; variant: GraphVariant }) {
  const navigate = useNavigate();
  return (
    <GraphPage
      engine={engine}
      variant={variant}
      onBack={() => navigate("/")}
    />
  );
}

function App() {
  const [isReady, setIsReady] = useState(false);
  const [loadError, setLoadError] = useState("");
  const engineRef = useRef<VisualizerEngine | null>(null);
  const createModuleRef = useRef<CreateVisualizerModule | null>(null);

    // ===  Wasmモジュールの読み込み ===
    useEffect(() => {
      let retryCount = 0;
      const maxRetries = 50; 
  
      const checkAndLoad = async () => {
        // index.html で読み込まれた core.js が createVisualizerModule を定義するのを待つ
        if (typeof globalThis.createVisualizerModule !== 'function') {
          if (retryCount < maxRetries) {
            retryCount++;
            setTimeout(checkAndLoad, 100);
          } else {
            setLoadError("Timeout: 'createVisualizerModule' is not defined. core.js failed to load.");
          }
          return;
        }
  
        try {
          if (!createModuleRef.current) {
            createModuleRef.current = globalThis.createVisualizerModule ?? null;
          }
          if (!createModuleRef.current) {
            setLoadError("createVisualizerModule が読み込めませんでした。");
            return;
          }
          const module = await createModuleRef.current();
          
          // C++のクラス名 "VisualizerEngine" をインスタンス化
          if (!module.VisualizerEngine) {
              throw new Error("VisualizerEngine class not found in Wasm. Did you rebuild?");
          }
          
          engineRef.current = new module.VisualizerEngine();
          setIsReady(true);
        } catch (e) {
          console.error("Wasm Init Error:", e);
          setLoadError(`Wasm Error: ${e instanceof Error ? e.message : String(e)}`);
        }
      };
      
      checkAndLoad();
  }, []);

  // エラー時の表示
  if (loadError) return (
    <div style={{ color: 'red', padding: 20, fontFamily: 'sans-serif' }}>
        <h2>System Error</h2>
        <p>{loadError}</p>
    </div>
  );

  // ロード中表示
  if (!isReady) return (
    <div style={{ display: 'flex', justifyContent: 'center', alignItems: 'center', height: '100vh' }}>
      <h3>Wasmエンジンを起動中...</h3>
    </div>
  );

  return (
    <BrowserRouter>
    <Routes>
      {/* URLが '/' のときはメニューを表示 */}
      <Route path="/" element={<MainMenu/>}/>

      {/* URLが '/brainfuck' のときはビジュアライザを表示 */}
      <Route path="/brainfuck" element={<BrainfuckWrapper engine={engineRef.current!} />} />

      {/* グラフ探索。1ページ1アルゴリズム */}
      <Route path="/graph/bfs" element={<GraphWrapper engine={engineRef.current!} variant="bfs" />} />
      <Route path="/graph/dfs" element={<GraphWrapper engine={engineRef.current!} variant="dfs" />} />
      <Route path="/graph/dijkstra" element={<GraphWrapper engine={engineRef.current!} variant="dijkstra" />} />

      <Route path="/automaton" element={<GraphWrapper engine={engineRef.current!} variant="automaton" />} />

      {/* 木 */}
      <Route path="/tree/bst" element={<TreeWrapper engine={engineRef.current!} variant="bst" />} />
      <Route path="/tree/heap" element={<TreeWrapper engine={engineRef.current!} variant="heap" />} />
      <Route path="/tree/trie" element={<TreeWrapper engine={engineRef.current!} variant="trie" />} />
      <Route path="/tree/huffman" element={<TreeWrapper engine={engineRef.current!} variant="huffman" />} />
      <Route path="/tree/avl" element={<TreeWrapper engine={engineRef.current!} variant="avl" />} />
      <Route path="/tree/btree" element={<TreeWrapper engine={engineRef.current!} variant="btree" />} />

      {/* 描くだけのページ。メニューには載せないが、レイアウトとパッキングの
          回帰を目視確認する手段としてルートは残す */}
      <Route path="/graph" element={<GraphWrapper engine={engineRef.current!} variant="plain" />} />

      {/* 定義の無いパスは真っ白になるので、トップへ送る */}
      <Route path="*" element={<Navigate to="/" replace />} />
    </Routes>

    </BrowserRouter>
  );
}

export default App;