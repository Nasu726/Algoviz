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
    let onWheel: (e: WheelEvent) => void;

    const initPixi = async () => {
      await app.init({ width: 600, height: 400, backgroundColor: 0xfcfcfc });
      if (isCancelled) return;
      containerRef.current!.appendChild(app.canvas);

      // カメラの役割を果たす「世界コンテナ」を作成
      const world = new PIXI.Container();
      app.stage.addChild(world);

      // ノードとエッジは世界コンテナの中に入れる
      const edgeGraphics = new PIXI.Graphics();
      world.addChild(edgeGraphics);

      const nodeContainer = new PIXI.Container();
      world.addChild(nodeContainer);

      const nodeSprites: PIXI.Sprite[] = [];
      const g = new PIXI.Graphics();
      g.circle(0, 0, 10).fill(0xffffff);
      const circleTexture = app.renderer.generateTexture(g);

      const fpsText = new PIXI.Text({ text: 'FPS: 0', style: { fontSize: 16, fill: 0x000000 } });
      fpsText.x = 10;
      fpsText.y = 20;
      app.stage.addChild(fpsText); // FPSは画面に固定したいのでworldではなくstageに置く

      // --- ドラッグ操作（パン）の実装 ---
      app.stage.eventMode = 'static'; // イベントを受け取る設定
      app.stage.hitArea = new PIXI.Rectangle(0, 0, 600, 400); // 画面全体でクリック判定
      
      let isDragging = false;
      let lastPos = { x: 0, y: 0 };

      app.stage.on('pointerdown', (e) => {
        isDragging = true;
        lastPos = { x: e.global.x, y: e.global.y };
      });
      app.stage.on('pointermove', (e) => {
        if (!isDragging) return;
        const dx = e.global.x - lastPos.x;
        const dy = e.global.y - lastPos.y;
        world.position.x += dx;
        world.position.y += dy;
        lastPos = { x: e.global.x, y: e.global.y };
      });
      app.stage.on('pointerup', () => (isDragging = false));
      app.stage.on('pointerupoutside', () => (isDragging = false));

      // --- ホイール操作（ズーム）の実装 ---
      onWheel = (e: WheelEvent) => {
        e.preventDefault(); // 画面全体がスクロールされるのを防ぐ
        const zoomFactor = 1.1;
        const scaleChange = e.deltaY < 0 ? zoomFactor : 1 / zoomFactor;

        // マウスカーソルの位置に向かってズームするための計算
        const rect = app.canvas.getBoundingClientRect();
        const mouseX = e.clientX - rect.left;
        const mouseY = e.clientY - rect.top;

        const oldScale = world.scale.x;
        let newScale = oldScale * scaleChange;
        newScale = Math.max(0.05, Math.min(newScale, 10)); // ズーム制限(0.05倍〜10倍)
        const actualScaleChange = newScale / oldScale;

        world.position.x = mouseX - (mouseX - world.position.x) * actualScaleChange;
        world.position.y = mouseY - (mouseY - world.position.y) * actualScaleChange;
        world.scale.set(newScale);
      };
      app.canvas.addEventListener('wheel', onWheel);

      // --- カリング判定用の関数 ---
      const isVisible = (x: number, y: number) => {
        // 世界座標を画面座標に変換
        const screenX = x * world.scale.x + world.position.x;
        const screenY = y * world.scale.y + world.position.y;
        // 画面外（余裕を持たせて-20〜広めに判定）かチェック
        return screenX >= -50 && screenX <= 650 && screenY >= -50 && screenY <= 450;
      };

      // --- ゲームループ ---
      app.ticker.add(() => {
        engine.step();
        const state = engine.getState({});
        const nodeArray = new Float32Array(state.nodes);
        const edgeArray = new Float32Array(state.edges);

        const nodeCount = nodeArray.length / 3;
        while (nodeSprites.length < nodeCount) {
          const sprite = new PIXI.Sprite(circleTexture);
          sprite.anchor.set(0.5);
          nodeSprites.push(sprite);
          nodeContainer.addChild(sprite);
        }

        // ノードの更新とカリング
        let visibleNodeCount = 0;
        for (let i = 0; i < nodeArray.length; i += 3) {
          const x = nodeArray[i];
          const y = nodeArray[i + 1];
          const colorId = nodeArray[i + 2];
          const sprite = nodeSprites[i / 3];

          if (isVisible(x, y)) {
            sprite.visible = true; // 画面内なら表示
            sprite.x = x;
            sprite.y = y;
            sprite.tint = colorId === 0 ? 0x3498db : 0xe74c3c;
            visibleNodeCount++;
          } else {
            sprite.visible = false; // 画面外なら非表示（GPUに送らない！）
          }
        }

        // エッジの更新とカリング
        edgeGraphics.clear();
        for (let i = 0; i < edgeArray.length; i += 3) {
          const fromIdx = edgeArray[i];
          const toIdx = edgeArray[i + 1];
          const fx = nodeArray[fromIdx * 3];
          const fy = nodeArray[fromIdx * 3 + 1];
          const tx = nodeArray[toIdx * 3];
          const ty = nodeArray[toIdx * 3 + 1];

          // どちらかのノードが画面内なら線を引く
          if (isVisible(fx, fy) || isVisible(tx, ty)) {
            edgeGraphics.moveTo(fx, fy);
            edgeGraphics.lineTo(tx, ty);
          }
        }
        // ズームしても線の太さが変わらないように調整 (2 / scale)
        edgeGraphics.stroke({ width: 2 / world.scale.x, color: 0x999999 });

        fpsText.text = `FPS: ${Math.round(app.ticker.FPS)} / Visible: ${visibleNodeCount}`;
      });
    };

    initPixi();

    return () => {
      isCancelled = true;
      if (onWheel && app.canvas) app.canvas.removeEventListener('wheel', onWheel);
      app.stage.destroy({ children: true });
      app.destroy({ removeView: true });
    };
  }, [engine]);

  return (
    <div ref={containerRef} style={{ border: '1px solid #ccc', marginTop: '20px', width: '600px', height: '400px' }} />
  );
};