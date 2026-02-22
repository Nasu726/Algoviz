import React, { useEffect, useRef } from 'react';
import * as PIXI from 'pixi.js';

interface GraphRendererProps {
  engine: any; 
}

export const GraphRenderer: React.FC<GraphRendererProps> = ({ engine }) => {
  const containerRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (!containerRef.current || !engine) return;

    const app = new PIXI.Application();
    let isCancelled = false;

    const initPixi = async () => {
      // 1. PixiJSアプリケーションの初期化 (v8は非同期)
      await app.init({ width: 600, height: 400, backgroundColor: 0xfcfcfc });
      if (isCancelled) return;
      containerRef.current!.appendChild(app.canvas);

      // 2. ノード用の「テクスチャ（画像）」を1つだけ作る
      // ※毎フレーム円を描くのではなく、1回描いた円をスタンプのように使い回すための準備
      const g = new PIXI.Graphics();
      g.circle(0, 0, 10).fill(0xffffff); 
      const circleTexture = app.renderer.generateTexture(g);

      // 3. 描画用のコンテナと配列を用意
      const edgeGraphics = new PIXI.Graphics();
      app.stage.addChild(edgeGraphics);

      const nodeContainer = new PIXI.Container();
      app.stage.addChild(nodeContainer);
      const nodeSprites: PIXI.Sprite[] = [];

      // FPS表示テキスト
      const fpsText = new PIXI.Text({ text: 'FPS: 0', style: { fontSize: 16, fill: 0x000000 } });
      fpsText.x = 10;
      fpsText.y = 20;
      app.stage.addChild(fpsText);

      // 4. 最強のゲームループ (PixiJSのTickerを使用)
      app.ticker.add(() => {
        // C++の計算を進めてメモリを受け取る
        engine.step();
        const state = engine.getState({});
        const nodeArray = new Float32Array(state.nodes);
        const edgeArray = new Int32Array(state.edges);

        // --- ノードの更新 (スプライトの座標と色を変えるだけ) ---
        const nodeCount = nodeArray.length / 3;
        
        // 足りない分のスプライト（スタンプ）を初回だけ生成
        while (nodeSprites.length < nodeCount) {
          const sprite = new PIXI.Sprite(circleTexture);
          sprite.anchor.set(0.5); // 中心を基準にする
          nodeSprites.push(sprite);
          nodeContainer.addChild(sprite);
        }

        // 座標と色を更新
        for (let i = 0; i < nodeArray.length; i += 3) {
          const x = nodeArray[i];
          const y = nodeArray[i + 1];
          const colorId = nodeArray[i + 2];
          
          const sprite = nodeSprites[i / 3];
          sprite.x = x;
          sprite.y = y;
          // 白い円形テクスチャに色を乗せる (Tint)
          sprite.tint = colorId === 0 ? 0x3498db : 0xe74c3c;
        }

        // --- エッジの更新 (一度クリアして線を引き直す) ---
        edgeGraphics.clear();
        for (let i = 0; i < edgeArray.length; i += 3) {
          const fromIndex = edgeArray[i];
          const toIndex = edgeArray[i + 1];
          const fromX = nodeArray[fromIndex * 3];
          const fromY = nodeArray[fromIndex * 3 + 1];
          const toX = nodeArray[toIndex * 3];
          const toY = nodeArray[toIndex * 3 + 1];

          edgeGraphics.moveTo(fromX, fromY);
          edgeGraphics.lineTo(toX, toY);
        }
        edgeGraphics.stroke({ width: 2, color: 0x999999 });

        // FPSの更新
        fpsText.text = `FPS: ${Math.round(app.ticker.FPS)}`;
      });
    };

    initPixi();

    // コンポーネントが消える時の後処理
    return () => {
      isCancelled = true;
      app.stage.destroy({ children: true });
      app.destroy({ removeView: true });
    };
  }, [engine]);

  return (
    // Canvasの代わりにDivを置き、そこにPixiJSがCanvasを注入する
    <div ref={containerRef} style={{ border: '1px solid #ccc', marginTop: '20px', width: '600px', height: '400px' }} />
  );
};