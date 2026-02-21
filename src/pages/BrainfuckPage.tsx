import React, { useEffect, useState, useRef } from 'react';
import { useInterval } from 'react-use';
import { Popup } from '../components/ui/popup';
import { useKeyboardShortcuts } from '../hooks/keyboardShortcut';
import { TapeViewer } from '../components/visualizers/TapeViewer';

// App.tsxの「型定義」をそのままコピーしてここに貼る
interface TapeCell { index: number; value: number; exists: boolean; name: string; }
interface VisualizerState {
  pc: number;
  ptr: number;
  tape: TapeCell[];     // 配列であることを期待
  output: string;
  stepCount: bigint;   // 任意項目にしておく
  code: string;
  isError: boolean;
  errorMessage: string;
}

// ★大事：Props（親から受け取るもの）を定義
interface BrainfuckPageProps {
  engine: any;       // Wasmのインスタンス
  onBack: () => void; // メニューに戻るための命令
}

export const BrainfuckPage: React.FC<BrainfuckPageProps> = ({ engine, onBack }) => {
  // コード・入出力
  const [code, setCode] = useState("++++++++++[>+++++++>++++++++++>+++>+<<<<-]>++.>+.+++++++..+++.>++.<<+++++++++++++++.>.+++.------.--------.>+.>.");
  const [input, setInput] = useState("");
  const [output, setOutput] = useState("");
  const [editorMode, setEditorMode] = useState(true);
  
  // ビジュアライザの状態
  const [state, setState] = useState<VisualizerState | null>(null);
  const [isPlaying, setIsPlaying] = useState(false);
  const [delay, setDelay] = useState(300);
  const [viewSize, setViewSize] = useState(20);
  const [cameraStart, setCameraStart] = useState(-10.0);
  const [autoScroll, setAutoScroll] = useState(true);
  const tapeContainerRef = useRef<HTMLDivElement>(null);
  const CELL_WIDTH = 60;

  // ユーザー操作
  const [isDragging, setIsDragging] = useState(false);
  const [isMobile, setIsMobile] = useState(window.innerWidth <= 768);
  const [lastTouchX, setLastTouchX] = useState<number | null>(null);
  const [isHelpPopupOpen, setIsHelpPopupOpen] = useState(false);

  const textAreaRef = useRef<HTMLTextAreaElement>(null);
  const highlightDivRef = useRef<HTMLDivElement>(null);
  const highlightSpanRef = useRef<HTMLSpanElement>(null);

  // スマホサイズの画面かどうかを検知
  useEffect(() => {
    const handleResize = () => setIsMobile(window.innerWidth <= 768);
    window.addEventListener('resize', handleResize);
    return () => window.removeEventListener('resize', handleResize);
  }, []);

  // エラー監視用
  useEffect(() => {
    // stateが存在し、かつエラーフラグが立っている時だけ実行
    if (state && state.isError) {
      setIsPlaying(false); // 停止
      
      // 画面描画が終わるのを一瞬待ってからアラートを出す（ここがコツ）
      const timer = setTimeout(() => {
        window.alert(state.errorMessage || "Runtime Error");
      }, 10);
      
      return () => clearTimeout(timer);
    }
  }, [state]); // stateが変わるたびにチェックする

  // ===  自動ロード (準備完了時に実行) ===
  useEffect(() => {
    if (engine && !state) {
      handleLoad();
    }
  }, [engine]);

  // === ResizeObserver による画面サイズ監視を追加 ===
  useEffect(() => {
    if (!tapeContainerRef.current) return;
    const observer = new ResizeObserver((entries) => {
      for (let entry of entries) {
        // テープエリアの幅に合わせて表示セル数を計算（+4 はスクロール時の余白）
        const newSize = Math.ceil(entry.contentRect.width / (CELL_WIDTH+4));
        setViewSize(newSize);
      }
    });
    observer.observe(tapeContainerRef.current);
    return () => observer.disconnect();
  }, [CELL_WIDTH]);

  // コード編集を監視
  useEffect(() => {
    if (isPlaying) setIsPlaying(false);
    setEditorMode(true);
  }, [code]);

  // === 自動スクロール ===
  useEffect(() => {
    if (state && autoScroll) setCameraStart(state.ptr - (viewSize+1) / 2);
  }, [autoScroll, viewSize]);

  // どんな描画更新があっても、強制的に背面divのスクロールを手前と一致させる
  useEffect(() => {
    if (highlightDivRef.current && textAreaRef.current) {
      highlightDivRef.current.scrollTop = textAreaRef.current.scrollTop;
      highlightDivRef.current.scrollLeft = textAreaRef.current.scrollLeft;
    }
  }); 

  // プログラムカウンタ(pc)が動いたときの自動スクロール
  useEffect(() => {
    if (!autoScroll) return;

    if (state && highlightSpanRef.current && highlightDivRef.current && textAreaRef.current) {
      // 1. ハイライトの span が見えるように背景 div をスクロールする
      highlightSpanRef.current.scrollIntoView({ 
        behavior: 'auto', 
        block: 'nearest', 
        inline: 'nearest' 
      });
      
      // 2. 背景 div のスクロール位置を、手前の textarea にも同期させる
      textAreaRef.current.scrollTop = highlightDivRef.current.scrollTop;
      textAreaRef.current.scrollLeft = highlightDivRef.current.scrollLeft;
    }
  }, [state?.pc, autoScroll]); // pc が変わるたびに実行

  // === 実行ループ ===
  useInterval(() => {
    if (isPlaying && engine) {
      stepExecution();
    }
  }, isPlaying ? delay : null);

  const backToMenu = () => {
    if (window.confirm("ビジュアライザ一覧へ戻りますか？（未保存の内容は失われます）")){
      onBack();
    }
  };

  const saveCode = () => {
    if (window.confirm(`作成したコードを保存しますか？`)) {
      const blob = new Blob([code], { type: 'text/plain' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = 'code.bf';
      a.click();
      URL.revokeObjectURL(url);
    }
  };

  // 1ステップ実行
  const stepExecution = () => {
    if (!engine) return;

    try {
      const result = engine.step();
      const tempState = engine.getState({ start: cameraStart, range: viewSize });

      // デバッグ用: 期待通りのデータが来ているかコンソールで確認できるようにする
      console.log("Engine State:", tempState);

      let nextCameraStart = cameraStart;
      if (autoScroll) {
        nextCameraStart = tempState.ptr - (viewSize+1) / 2;
      }

      const newState = engine.getState({ start: nextCameraStart, range: viewSize });

      setState(newState);
      setCameraStart(nextCameraStart);
      
      // 出力取得の優先順位: getOutput関数 > state.output > 空文字
      if (engine.getOutput) {
          setOutput(engine.getOutput());
      } else if (newState && typeof newState.output === 'string') {
          setOutput(newState.output);
      }

      if (!result) setIsPlaying(false);
    } catch (e) {
      console.error("Execution Error:", e);
      setIsPlaying(false);
    }
  };

  // 実行ボタンを押したときの動作
  const executeButton = () => {
    if(!engine) return;
    if(state && (code != state.code || state.stepCount===0n)){
      handleLoad();
    }
    setIsPlaying(!isPlaying);
  };

  // ステップ実行時の動作
  const stepButton = () => {
    if(!engine) return;
    if(state && (code !== state.code || state.stepCount===0n)){
      handleLoad();
    }
    stepExecution();
  }

  // ステップバック時の動作
  const stepBack = () => {
    if(!engine) return;
    if(state && (code !== state.code || state.stepCount===0n)){
      handleLoad();
      return;
    }

    try {
      engine.stepBack();
      const currentState = engine.getState({ start: cameraStart, range: viewSize });

      if(autoScroll) {
        setCameraStart(currentState.ptr - (viewSize+1) / 2);
        setState(engine.getState({ start: currentState.ptr - viewSize / 2, range: viewSize }));
      } else {
        setState(currentState);
      }

      if (engine.getOutput) {
        setOutput(engine.getOutput());
      } else {
        setOutput(currentState.output);
      }

    } catch (e) {
      console.error("StepBack Error:", e);
      setIsPlaying(false);
    }
  };

  // ロード処理
  const handleLoad = () => {
    if (!engine) return;
    try {
      engine.load(code, input);
      setCameraStart(-(viewSize+1)/2);
      const newState = engine.getState({ start: cameraStart, range: viewSize });
      setState(newState);
      setOutput("");
      setIsPlaying(false);
      setEditorMode(false);
    } catch (e) {
      console.error("Load Error:", e);
    }
  };

  // === スタイル ===
  const commonTextStyle: React.CSSProperties = {
    width: '100%', height: '100%',
    fontFamily: 'Consolas',
    fontSize: '20px',
    lineHeight: '1.5',
    padding: '10px',
    paddingBottom: '40px',
    boxSizing: 'border-box',
    border: 'none',
    resize: 'none',
    outline: 'none',
    whiteSpace: 'pre',
    overflow: 'auto',
    overscrollBehavior: 'none'
  };

  // 1. 整数部分：C++に渡す「開始インデックス」
  const baseIndex = Math.floor(cameraStart);

  // 2. 小数部分：表示をずらす量 (ピクセル変換)
  const offsetPx = (cameraStart - baseIndex) * (CELL_WIDTH);

  // 3. 画面描画用のデータを取得
  let tapeData: TapeCell[] = [];
  if (engine) {
    const displayState = engine.getState({ start: baseIndex, range: viewSize + 2 });
    tapeData = displayState.tape;
  }

  // === キーボードショートカット === 
  useKeyboardShortcuts({
    onEsc: !isHelpPopupOpen ? backToMenu : undefined,
    onHelp: () => setIsHelpPopupOpen(!isHelpPopupOpen),
    onSave: !isHelpPopupOpen ? handleLoad: undefined,
    onSaveFile: !isHelpPopupOpen ? saveCode: undefined,
    onPlayPause: !isHelpPopupOpen ? executeButton : undefined,
    onFocus: () => !isHelpPopupOpen ? setAutoScroll(!autoScroll) : null,
    onStepNext: !isHelpPopupOpen ? stepButton : undefined,
    onStepBack: !isHelpPopupOpen ? stepBack : undefined,
    onSpeedUp: () => {
      if(isHelpPopupOpen) return;
      if(delay>=10) {
        setDelay(Math.max(0, ((-Math.sqrt(1000*delay)+100)**2)/1000));
      } else {
        setDelay(0);
      };
    },
    onSpeedDown: () => {
      if(isHelpPopupOpen) return;
      setDelay(Math.max(0, ((-Math.sqrt(1000*delay)-100)**2)/1000));
    },
  });

  return (
    <div style={{ 
      display: 'flex', flexDirection: 'column', 
      height: '100vh', width: '100vw', 
      margin: 0, overflow: 'hidden', fontFamily: 'sans-serif' 
    }}>
  
      {/* === ヘッダー === */}
      <div style={{ 
        display: 'flex', 
        justifyContent: 'space-between', 
        alignItems: 'center', 
        padding: isMobile ? '8px 10px' : '10px 20px', 
        backgroundColor: '#263238', 
        color: 'white' 
      }}>
        <button onClick={backToMenu} style={{ cursor: 'pointer' }}>
          ◀ 戻る
        </button>
        <h2 style={{ margin: 0, fontSize: isMobile ? '10px' : '18px' }}>Brainfuck Visualizer</h2>
        <button onClick={() => setIsHelpPopupOpen(true)} style={{ cursor:'pointer', fontWeight: "bold" }}>
          ヘルプ ❓
        </button>
      </div>
      {/* === [1] 上部: テープ表示エリア === */}
      <div 
        ref={tapeContainerRef}
        style={{ 
          flex: '5', 
          borderBottom: '1px solid #ccc',
          backgroundColor: '#fcfcfc',
          display: 'flex', flexDirection: 'column',
          alignItems: 'center', justifyContent: 'center',
          position: 'relative',
          cursor: isDragging ? 'grabbing' : 'grab',
          overflow: 'hidden',
          userSelect: 'none',
          touchAction: 'none'
        }}
        onTouchStart={(e) => {
          setIsDragging(true);
          setLastTouchX(e.touches[0].clientX);
        }}
        onTouchMove={(e) => {
          if(!isDragging || lastTouchX == null) return;
          setAutoScroll(false);
          const currentX = e.touches[0].clientX;
          const deltaX = currentX - lastTouchX;
          setCameraStart(Math.max(-viewSize, Math.min(30000, cameraStart - deltaX/(CELL_WIDTH))));
          setLastTouchX(currentX);
        }}
        onTouchEnd={() => {
          setIsDragging(false);
          setLastTouchX(null);
        }}
        onMouseDown={() => {
          setIsDragging(true);
        }}
        onMouseUp={() => {
          setIsDragging(false);
        }}
        onMouseLeave={() => {
          setIsDragging(false);
        }}
        onMouseMove={(e) => {
          if(!isDragging) return;
          setAutoScroll(false);
          const nextCameraStart = Math.max(-viewSize, Math.min(30000, cameraStart - e.movementX/(CELL_WIDTH)));
          setCameraStart(nextCameraStart);
        }}
      >
         { !state ? (
            <div>Ready (Press Load)</div>
         ) : (
          <TapeViewer
            tapeData={tapeData}
            ptr={state.ptr}
            stepCount={state.stepCount}
            offsetPx={offsetPx}
          />
         )}
      </div>
  
      {/* === [2] 下部: 操作部 === */}
      <div style={{ 
        flex: '6', 
        display: 'flex', flexDirection: 'column',
        backgroundColor: '#eee'
      }}>
  
        {/* ボタン群 */}
        <div style={{ 
          // flex: '1.3', 
          borderBottom: '1px solid #ccc',
          padding: '0 20px',
          display: 'flex', 
          flexWrap: 'wrap',
          justifyContent: 'center',
          alignItems: 'center', 
          gap: '10px',
          backgroundColor: '#f5f5f5',
        }}>
           <button onClick={handleLoad} style={{ padding: '8px 8px', fontWeight: 'bold', flexShrink: 0, whiteSpace: 'nowrap' }}>ロード</button>
           <button onClick={executeButton} disabled={!state} style={{ padding: '8px 12px', fontWeight: 'bold', flexShrink: 0, whiteSpace: 'nowrap' }}>
             {isPlaying ? "停止" : "実行"}
           </button>
           <button onClick={stepBack} disabled={!state || isPlaying} style={{ padding: '8px 8px', fontWeight: 'bold', flexShrink: 0, whiteSpace: 'nowrap' }}>戻る</button>
           <button onClick={stepButton} disabled={!state || isPlaying} style={{ padding: '8px 8px', fontWeight: 'bold', flexShrink: 0, whiteSpace: 'nowrap' }}>進む</button>
           <div style={{ 
             display: 'flex',       // グループの中身も横並びにする
             alignItems: 'center',  // 縦の真ん中で揃える
             gap: '10px',           // グループ内の要素の隙間
             flexShrink: 0          // グループ全体として縮まないようにする
           }}>
              <span style={{padding: '0px 0px 0px 0px', fontWeight: 'bold', flexShrink: 0, whiteSpace: 'nowrap' }}>実行速度
                <input type="range" min="0" max="1000" value={1000-Math.sqrt(1000*delay)} onChange={(e) => {const x=Number(e.target.value);setDelay((x-1000)*(x-1000)/1000)}} style={{ marginLeft: '0.5em' }}/>
              </span>
              <label style={{ display: 'flex', alignItems: 'center', fontWeight: 'bold', userSelect: 'none', flexShrink: 0, whiteSpace: 'nowrap'}}>
                <input type='checkbox' checked={autoScroll} onChange={(e) => setAutoScroll(e.target.checked)} style={{ padding: '8px 8px'}}/>
                自動追従
              </label>
            </div>
        </div>
  
        {/* エディタ & I/O */}
        <div style={{ flex: '8', display: 'flex', flexDirection: 'row' }}>
  
          {/* コードエディタ */}
          <div style={{ flex: '6', borderRight: '1px solid #ccc', position: 'relative', backgroundColor: '#fff' }}>
             {state && !editorMode && (
               <div ref={highlightDivRef} style={{ ...commonTextStyle, position: 'absolute', top: 0, left: 0, pointerEvents: 'none', color: 'transparent', margin: 0, zIndex: 1 }}>
                 {code.substring(0, state.pc)}
                 <span ref={highlightSpanRef} style={{ backgroundColor: 'rgba(255, 162, 0, 0.3)', outline: '1px solid orange' }}>
                   {code[state.pc] || ' '}
                 </span>
                 {code.substring(state.pc + 1)}
               </div>
             )}
             <textarea 
                ref={textAreaRef} value={code} 
                onChange={(e) => setCode(e.target.value)}
                onScroll={(e) => {
                  if(highlightDivRef.current) {
                    highlightDivRef.current.scrollTop = e.currentTarget.scrollTop;
                    highlightDivRef.current.scrollLeft = e.currentTarget.scrollLeft;
                  }
                }}
                style={{ ...commonTextStyle, background: 'transparent', position: 'absolute', top: 0, left: 0, margin: 0, zIndex: 2 }} 
                spellCheck={false} placeholder="Brainfuck Code"
             />
          </div>
  
          {/* I/O */}
          <div style={{ flex: '4', display: 'flex', flexDirection: 'column' }}>
            <div style={{ flex: '1', borderBottom: '1px solid #ccc', display: 'flex', flexDirection: 'column' }}>
               <span style={{ fontSize: '11px', fontWeight: 'bold', padding: '4px 10px', background: '#e0e0e0' }}>標準入力</span>
               <textarea value={input} onChange={(e) => setInput(e.target.value)} style={{ ...commonTextStyle, backgroundColor: '#fff' }} />
            </div>
            <div style={{ flex: '1', display: 'flex', flexDirection: 'column' }}>
               <span style={{ fontSize: '11px', fontWeight: 'bold', padding: '4px 10px', background: '#e0e0e0' }}>標準出力</span>
               <textarea readOnly value={output} style={{ ...commonTextStyle, backgroundColor: '#fff', color: '#000' }} />
            </div>
          </div>
  
        </div>
      </div>
      <Popup 
        title={"ヘルプ"}
        isOpen={isHelpPopupOpen}
        onClose={() => setIsHelpPopupOpen(false)}
      >
        <h3>1. ビジュアライザの仕様</h3>
        <h4>メモリテープ</h4>
        <ul>
          <li><b>セル</b>：データの箱。書いてある情報は上から順に以下の通り
          <ul>
            <li><b>値</b>：セルの値。0～255の符号なし整数で表され、初期値は0</li>
            <li><b>文字</b>：セルの値を文字コードと見たときに対応する文字。C++のchar型に準拠しているが、ASCIIの制御文字にあたる文字はそれを表す文字列が表示される</li>
            <li><b>番地</b>：セルの番地。0～29999まであり、テープ下の「ポインタ位置」は実行中のプログラムが指すこれを表す。テープは循環していないので、範囲外参照はエラーとなる。</li>
          </ul>
          </li>
          <li><b>ポインタ位置</b>：実行中のプログラムが指しているセルの番地。初期値は0</li>
          <li><b>実行ステップ数</b>：実行された命令の数</li>
        </ul>
        <h4>各種ボタン・操作</h4>
        <ul>
          <li><b>ロード</b>：ビジュアライザの状態をプログラム実行前の状態にする</li>
          <li><b>実行/停止</b>：プログラムを実行/停止できる</li>
          <li><b>戻る</b>：プログラムの１つ前の命令を実行する前の状態に戻す。ステップバック</li>
          <li><b>進む</b>：プログラムの次の命令を読んで状態を更新する。ステップ実行</li>
          <li><b>実行速度</b>：実行速度を変更できる。バーを一番左にすると1秒ごとに1ステップ実行され、一番右にすると限りなく高速に実行される</li>
          <li><b>自動追従</b>：チェックボックスにチェックが入っている間、ポインタが指すセルを自動でフォーカスして追う。チェックを外すか画面上部をクリックして左右に動かすと手動制御に切り替わる</li>
        </ul>
        <h4>画面下部</h4>
        <ul>
          <li><b>コードエディタ</b>：コードの編集ができる。プログラム実行時は次の命令の位置がハイライトされる</li>
          <li><b>標準入力</b>：標準入力を与えることができる。2byte以上で表現される文字は1byteずつ読み取られるが、値はUTF-8の内部表現に依存する。(例：ö(246) → 195 182)</li>
          <li><b>標準出力</b>：標準出力の結果が表示される。表示される文字は、出力されたバイト列をUTF-8として解釈したときの値となる。(例：195 182 → ö)</li>
        </ul>
        <h4>その他細かい仕様</h4>
        <ul>
          <li>コードが編集されると次回実行時に自動でロードされる。現在の状態を確認しながら編集でき、実行時は自動でリロードされてスムーズな体験を提供する</li>
          <li>過去1000ステップ分の実行履歴を保持するため、ステップバックは1000回まで可能。</li>
        </ul>

        <h3>2. Brainfuckの文法</h3>
        <p>使える命令は以下の8つ。全て半角記号。これらの記号以外は無視される</p>
        <ul>
          <li><b>＋</b>： セルの値のインクリメント</li>
          <li><b>ー</b>： セルの値のデクリメント</li>
          <li><b>＞</b>： ポインタのインクリメント(右移動)</li>
          <li><b>＜</b>： ポインタのデクリメント(左移動)</li>
          <li><b>［</b>： ループ開始。ポインタが指すセルの値が0なら対応する閉じカッコ( ］)までジャンプする。while(tape[ptr]＞0)と等価</li>
          <li><b>］</b>： ループ閉じ。ポインタが指すセルの値が0でないなら対応する開きカッコ(［ )までジャンプする。</li>
          <li><b>，</b>： 標準入力を1byteだけ受け取る。2byte以上で表現される文字は1byteずつ読み取られるが、値はUTF-8の内部表現に依存する。(例：ö(246=0xF6) → 195 182)</li>
          <li><b>．</b>： 1byteの標準出力。ポインタが指すセルの値を表すビット列を出力する。()</li>
        </ul>
        <h4>このビジュアライザでのみ使える特別な命令</h4>
        <ul>
          <li><b>!</b>：仮の変数宣言。英文字と数字(先頭は不可)、アンダーバーからなる文字列を「!」で挟むと、宣言した位置のセルに名前を付けられる(値は上書きされない)。既に名前が付いたセルの番地でもう一度変数宣言をすると名前が上書きされる。 例：!Variable_1!<br/>*有効な文字列の正規表現： ^([A-Za-z_][0-9A-Za-z_]*)?$ </li>
        </ul>
        
        <h3>3. ショートカットキー</h3>
        <ul>
          <li><b>Esc</b>：ビジュアライザ選択画面へ戻る</li>
          <li><b>Ctrl + Enter</b>：実行/一時停止</li>
          <li><b>Ctrl + H</b>：ヘルプを開く</li>
          <li><b>Ctrl + S</b>：コードを保存してロード</li>
          <li><b>Ctrl + Alt + S</b>：コードをファイルとして保存</li>
          <li><b>Ctrl + F</b>：自動追従/手動追従の切り替え</li>
          <li><b>Ctrl + ←</b>：戻る/ステップバック</li>
          <li><b>Ctrl + →</b>：進む/ステップ実行</li>
          <li><b>Ctrl + ↑</b>：実行速度アップ</li>
          <li><b>Ctrl + ↓</b>：実行速度ダウン</li>
        </ul>
      </Popup>
    </div>
  );
};