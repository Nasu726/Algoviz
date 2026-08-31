import React, { useEffect, useState, useRef } from 'react';
import { VisualizerShell } from '../components/ui/VisualizerShell';
import { PlaybackControls } from '../components/ui/PlaybackControls';
import { useKeyboardShortcuts } from '../hooks/keyboardShortcut';
import { usePlayback } from '../hooks/usePlayback';
import { useViewportWidth } from '../hooks/useViewportWidth';
import { TapeViewer } from '../components/visualizers/TapeViewer';
import type { VisualizerEngine, BrainfuckState, TapeCell } from '../types/engine';

// ★大事：Props（親から受け取るもの）を定義
interface BrainfuckPageProps {
  engine: VisualizerEngine; // Wasmのインスタンス
  onBack: () => void; // メニューに戻るための命令
}

export const BrainfuckPage: React.FC<BrainfuckPageProps> = ({ engine, onBack }) => {
  // コード・入出力
  const [code, setCode] = useState("+++++++++++[>++++++>+++++++++>++++++++>++++>+++>+<<<<<<-]>++++++.>++.+++++++..+++.>>.>-.<<-.<.+++.------.--------.>>>+.>-.");
  const [input, setInput] = useState("");
  const [output, setOutput] = useState("");
  const [editorMode, setEditorMode] = useState(true);
  
  // ビジュアライザの状態
  const [state, setState] = useState<BrainfuckState | null>(null);
  const [mod256, setModint] = useState(true);
  const [viewSize, setViewSize] = useState(20);
  const [cameraStart, setCameraStart] = useState(-10.0);
  const [autoScroll, setAutoScroll] = useState(true);
  const tapeContainerRef = useRef<HTMLDivElement>(null);
  const CELL_WIDTH = 60;

  // ユーザー操作
  const [isDragging, setIsDragging] = useState(false);
  // テープのセルが並ぶ幅で切る。レイアウトの3段階 (useLayoutTier) とは
  // 意味が違うので、しきい値は揃えない。
  const isMobile = useViewportWidth() <= 475;
  const [lastTouchX, setLastTouchX] = useState<number | null>(null);
  const [isHelpPopupOpen, setIsHelpPopupOpen] = useState(false);

  const { isPlaying, setIsPlaying, delay, setDelay, onSpeedUp, onSpeedDown } =
    usePlayback(() => stepExecution());

  const textAreaRef = useRef<HTMLTextAreaElement>(null);
  const highlightDivRef = useRef<HTMLDivElement>(null);
  const highlightSpanRef = useRef<HTMLSpanElement>(null);

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
    // setIsPlaying は useState の setter なので、依存に入れても再実行はされない
  }, [state, setIsPlaying]); // stateが変わるたびにチェックする

  // ステップ上限による中断の通知。state から導出するので、
  // 1ステップでも進める / 戻る / ロードすれば自動的に消える。
  const notice = state?.interrupted
    ? `ステップ上限 (${Number(state.stepLimit).toLocaleString()} ステップ) に達したため中断しました。「一気に実行」をもう一度押すと続きから実行します。`
    : "";

  // ===  自動ロード (準備完了時に実行) ===
  useEffect(() => {
    if (engine && !state) {
      handleLoad();
    }
    // Wasm エンジンが用意できた1回だけ走らせたい
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [engine]);

  // === ResizeObserver による画面サイズ監視を追加 ===
  useEffect(() => {
    if (!tapeContainerRef.current) return;
    const observer = new ResizeObserver((entries) => {
      for (const entry of entries) {
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
    // コードが編集されたときだけ反応する
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [code]);

  // === 自動スクロール ===
  useEffect(() => {
    if (state && autoScroll) setCameraStart(state.ptr - (viewSize+1) / 2);
    // 追従の切り替えと表示幅の変化にだけ反応する (実行中は各ステップ側で追従している)
    // eslint-disable-next-line react-hooks/exhaustive-deps
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
    // pc が動いたときだけスクロールし直す
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [state?.pc, autoScroll]);

  // 実行系が毎回やること。C++ から状態を読み直し、追従と出力を揃える。
  const syncState = () => {
    const newState = engine.getState<BrainfuckState>({ start: cameraStart, range: viewSize });
    setState(newState);
    if (autoScroll) setCameraStart(newState.ptr - (viewSize + 1) / 2);
    setOutput(engine.getOutput());
    return newState;
  };

  // コードが書き換わっていたら、あるいはまだ1手も進んでいなければ読み込み直す。
  // 読み込み直したかどうかを返す。ステップバックだけは、読み込み直した時点で
  // 「先頭に戻った」ことになるので、そのまま何もせず抜ける。
  const ensureLoaded = () => {
    if (state && (code !== state.code || state.stepCount === 0n)) {
      handleLoad();
      return true;
    }
    return false;
  };

  const runToEnd = () => {
    if (!engine) return;
    if (isPlaying) setIsPlaying(false);
    ensureLoaded();

    try {
        // C++側でステップ上限まで一気に回す。上限に達した場合は
        // interrupted が立ち、通知バナーで知らせる。
        engine.runToEnd();
        syncState();
    } catch (e) {
        console.error("RunToEnd Error:", e);
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
      const alive = engine.step();
      syncState();
      if (!alive) setIsPlaying(false);
    } catch (e) {
      console.error("Execution Error:", e);
      setIsPlaying(false);
    }
  };

  // 実行ボタンを押したときの動作
  const executeButton = () => {
    if (!engine) return;
    ensureLoaded();
    setIsPlaying(!isPlaying);
  };

  // ステップ実行時の動作
  const stepButton = () => {
    if (!engine) return;
    ensureLoaded();
    stepExecution();
  };

  // ステップバック時の動作
  const stepBack = () => {
    if (!engine) return;
    // 読み込み直したなら、それが「先頭に戻る」ことそのものなので何もしない
    if (ensureLoaded()) return;

    try {
      engine.stepBack();
      syncState();
    } catch (e) {
      console.error("StepBack Error:", e);
      setIsPlaying(false);
    }
  };

  // ロード処理
  const handleLoad = () => {
    if (!engine) return;
    try {
      engine.setAlgorithm("brainfuck");
      engine.setBrainfuckModint(mod256);
      engine.load(code, input);
      // setCameraStart は次のレンダーまで反映されないので、
      // getState には計算した値をそのまま渡す
      const newStart = -(viewSize+1)/2;
      setCameraStart(newStart);
      setState(engine.getState<BrainfuckState>({ start: newStart, range: viewSize }));
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
    backgroundColor: '#fff',
    color: '#000000',
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
    const displayState = engine.getState<BrainfuckState>({ start: baseIndex, range: viewSize + 2 });
    tapeData = displayState.tape;
  }

  // === キーボードショートカット === 
  useKeyboardShortcuts({
    onEsc: !isHelpPopupOpen ? onBack : undefined,
    onHelp: () => setIsHelpPopupOpen(!isHelpPopupOpen),
    onSave: !isHelpPopupOpen ? handleLoad: undefined,
    onSaveFile: !isHelpPopupOpen ? saveCode: undefined,
    onPlayPause: !isHelpPopupOpen ? executeButton : undefined,
    onFocus: () => !isHelpPopupOpen ? setAutoScroll(!autoScroll) : null,
    onStepNext: !isHelpPopupOpen ? stepButton : undefined,
    onStepBack: !isHelpPopupOpen ? stepBack : undefined,
    onSpeedUp: () => { if (!isHelpPopupOpen) onSpeedUp(); },
    onSpeedDown: () => { if (!isHelpPopupOpen) onSpeedDown(); },
  });

  const help = (
    <>
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
          <li><b>mod128/256</b>：セルの値が取りうる範囲を設定できる。mod128のときは0～127、mod256のときは0～255を取る。</li>
        </ul>
        <h4>画面下部</h4>
        <ul>
          <li><b>コードエディタ</b>：コードの編集ができる。プログラム実行時は次の命令の位置がハイライトされる</li>
          <li><b>標準入力</b>：標準入力を与えることができる。2byte以上で表現される文字は1byteずつ読み取られるが、値はUTF-8の内部表現に依存する。(例：ö(246) → 195 182)</li>
          <li><b>標準出力</b>：標準出力の結果が表示される。表示される文字は、出力されたバイト列をUTF-8として解釈したときの値となる。(例：195 182 → ö)</li>
        </ul>
        <h4>その他細かい仕様</h4>
        <ul>
          <li>EOF は -1 です。7bit モードでは127、8bit モードでは255にあたります。</li>
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
    </>
  );

  return (
    <VisualizerShell
      title="Brainfuck Visualizer"
      compact={isMobile}
      onBack={onBack}
      backConfirm="ビジュアライザ一覧へ戻りますか？（未保存の内容は失われます）"
      isHelpOpen={isHelpPopupOpen}
      setIsHelpOpen={setIsHelpPopupOpen}
      help={help}
    >
      {/* === [1] 上部: テープ表示エリア === */}
      <div 
        ref={tapeContainerRef}
        style={{ 
          flex: '5', 
          borderBottom: '1px solid #ccc',
          backgroundColor: '#fcfcfc',
          color: '#000000',
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
        {notice && (
          <div style={{
            position: 'absolute',
            top: '12px', left: '12px', right: '130px',
            zIndex: 20,
            display: 'flex', alignItems: 'center', gap: '10px',
            padding: '8px 12px',
            backgroundColor: '#fff8e1',
            border: '1px solid #ffb300',
            borderRadius: '4px',
            color: '#5d4037',
            fontSize: '13px',
          }}>
            <span style={{ flex: 1 }}>{notice}</span>
          </div>
        )}

        <div
          style={{
            position: 'absolute',
            top: '12px',
            right: '12px',
            zIndex: 20,
            userSelect: 'none',
          }}
        >
          <label style={{ display: 'inline-block', cursor: 'pointer' }}>
            <input
              type="checkbox"
              checked={mod256}
              onChange={(e) => {
                const checked = e.target.checked;
                setModint(checked);
                if (engine && engine.setBrainfuckModint) engine.setBrainfuckModint(checked);
                try {handleLoad();} catch { /* ロード失敗は次の実行時に再試行されるので握りつぶす */ }
              }}
              style={{ position: 'absolute', opacity: 0, width: 0, height: 0 }}
            />

            <div
              style={{
                position: 'relative',
                width: 100,
                height: 34,
                borderRadius: 999,
                backgroundColor: mod256 ? '#26a69a' : '#90a4ae',
                boxShadow: '0 2px 6px rgba(0,0,0,0.12)',
                display: 'flex',
                alignItems: 'center',
                padding: 4,
                boxSizing: 'border-box',
              }}
              aria-hidden
            >
              <div
                style={{
                  width: 28,
                  height: 28,
                  borderRadius: '50%',
                  backgroundColor: '#fff',
                  boxShadow: '0 1px 3px rgba(0,0,0,0.25)',
                  transform: mod256 ? 'translateX(64px)' : 'translateX(0)',
                  transition: 'transform 0.18s ease',
                  zIndex: 2,
                }}
              />
              <div
                style={{
                  position: 'absolute',
                  left: 0,
                  right: 0,
                  textAlign: 'center',
                  color: '#fff',
                  fontWeight: 700,
                  fontSize: 12,
                  pointerEvents: 'none',
                  zIndex: 1,

                  transform: mod256 ? 'translateX(-12px)' : 'translateX(12px)',
                  transition: 'transform 220ms cubic-bezier(.2,.8,.2,1)',
                  willChange: 'transform',
                  userSelect: 'none',

                }}
              >
                {mod256 ? 'mod 256' : 'mod 128'}
              </div>
            </div>
          </label>
        </div>

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
        backgroundColor: '#eee',
        color: '#000000',
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
          color: '#000000',
        }}>
           <PlaybackControls
             isPlaying={isPlaying}
             ready={!!state}
             delay={delay}
             onLoad={handleLoad}
             onPlayPause={executeButton}
             onStepBack={stepBack}
             onStepNext={stepButton}
             onRunToEnd={runToEnd}
             onDelayChange={setDelay}
             compact={isMobile}
           >
             <label style={{ display: 'flex', alignItems: 'center', fontWeight: 'bold', userSelect: 'none', flexShrink: 0, whiteSpace: 'nowrap'}}>
               <input type='checkbox' checked={autoScroll} onChange={(e) => setAutoScroll(e.target.checked)} style={{ padding: '8px 8px' }}/>
               自動追従
             </label>
           </PlaybackControls>
        </div>
  
        {/* エディタ & I/O */}
        <div style={{ flex: '8', display: 'flex', flexDirection: 'row' }}>
  
          {/* コードエディタ */}
          <div style={{ flex: '6', borderRight: '1px solid #ccc', position: 'relative', backgroundColor: '#fff', color: '#000' }}>
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
                spellCheck={false} placeholder="ここにコードを書く"
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
    </VisualizerShell>
  );
};