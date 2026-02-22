import { useEffect, useState, useRef } from 'react';
import { BrowserRouter, Routes, Route, useNavigate } from 'react-router-dom';

import { BrainfuckPage } from './pages/BrainfuckPage';
import { GraphPage } from './pages/GraphPage';

function MainMenu() {
  const navigate = useNavigate();
  return (
    <div style={{ 
      display: 'flex', flexDirection: 'column', 
      height: '100vh', width: '100vw',
      textAlign: 'center', alignItems: 'center',
      justifyContent: 'center',
      overflow: 'hidden', fontFamily: 'sans-serif' 
    }}>
      <h1 style={{ fontSize: '45px' }}>ビジュアライザへようこそ</h1>
      <h3>利用可能なビジュアライザ</h3>
      <button 
        onClick={() => navigate('/brainfuck')} 
        style={{ fontSize: '20px', padding: '10px 20px', border: '1px solid' }}
      >
        Brainfuck
      </button>
      <h4>他のビジュアライザはこれから追加されます</h4>
    </div>
  );
}

function BrainfuckWrapper({ engine }: { engine: any }) {
  const navigate = useNavigate();
  return (
    <BrainfuckPage 
      engine={engine} 
      onBack={() => navigate('/')} // ★ '/' (トップ) へ遷移
    />
  );
}

function GraphWrapper({engine}: {engine: any}){
  const navigate = useNavigate();
  return (
    <GraphPage
      engine={engine}
      onBack={() => navigate("/")}
    />
  );
}

function App() {
  const [isReady, setIsReady] = useState(false);
  const [loadError, setLoadError] = useState("");
  const engineRef = useRef<any>(null);
  const createModuleRef = useRef<any>(null);

    // ===  Wasmモジュールの読み込み ===
    useEffect(() => {
      let retryCount = 0;
      const maxRetries = 50; 
  
      const checkAndLoad = async () => {
        // index.html で読み込まれた core.js が createVisualizerModule を定義するのを待つ
        if (typeof (window as any).createVisualizerModule !== 'function') {
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
            createModuleRef.current = (window as any).createVisualizerModule;
          }
          const module = await createModuleRef.current();
          
          // C++のクラス名 "VisualizerEngine" をインスタンス化
          if (!module.VisualizerEngine) {
              throw new Error("VisualizerEngine class not found in Wasm. Did you rebuild?");
          }
          
          engineRef.current = new module.VisualizerEngine();
          setIsReady(true);
        } catch (e: any) {
          console.error("Wasm Init Error:", e);
          setLoadError(`Wasm Error: ${e.message}`);
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
      <Route path="/brainfuck" element={<BrainfuckWrapper engine={engineRef.current} />} />

      <Route path="/graph" element={<GraphWrapper engine={engineRef.current} />} />
    </Routes>

    </BrowserRouter>
  );
}

export default App;