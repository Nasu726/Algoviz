import * as PIXI from 'pixi.js';

export class PixiGraphApp {
    private app: PIXI.Application;
    private container: HTMLDivElement;
    private engine: any;
    
    // PixiJSのオブジェクト群
    private world!: PIXI.Container;
    private edgeGraphics!: PIXI.Graphics;
    private nodeContainer!: PIXI.Container;
    private nodeSprites: PIXI.Container[] = [];
    private circleTexture: PIXI.Texture | null = null;
    private fpsText!: PIXI.Text;
    
    // 状態管理フラグ
    private isInitialized = false;
    private isDestroyed = false;

    // マウス操作用
    private isDragging = false;
    private lastPos = { x: 0, y: 0 };

    constructor(container: HTMLDivElement, engine: any) {
        this.container = container;
        this.engine = engine;
        this.app = new PIXI.Application();
    }

    // ★ 初期化処理（Reactから呼ばれる）
    public async init() {
        await this.app.init({ width: 600, height: 400, backgroundColor: 0xfcfcfc });
        
        // 初期化を待っている間にReactがコンポーネントを破棄していたら、即座に終了する
        if (this.isDestroyed) {
            this.app.destroy({ removeView: true });
            return;
        }

        this.container.appendChild(this.app.canvas);
        
        this.world = new PIXI.Container();
        this.app.stage.addChild(this.world);

        this.edgeGraphics = new PIXI.Graphics();
        this.world.addChild(this.edgeGraphics);

        this.nodeContainer = new PIXI.Container();
        this.world.addChild(this.nodeContainer);

        // ノード用のコンテナ（箱）を100個あらかじめ作っておく
        for (let i = 0; i < 100; i++) {
            const nodeGroup = new PIXI.Container();

            // 1. 白抜きの円（Graphics）
            const bg = new PIXI.Graphics();
            bg.circle(0, 0, 14);
            bg.fill(0xffffff); // 中身は白
            bg.stroke({ width: 3, color: 0xcccccc }); // 枠線（最初はグレー）
            nodeGroup.addChild(bg); // 箱に入れる

            // 2. ノード番号のテキスト（Text）
            const text = new PIXI.Text({
                text: i.toString(),
                style: { fontSize: 14, fill: 0x333333, fontWeight: 'bold' }
            });
            text.anchor.set(0.5); // テキストを中央揃えにする
            nodeGroup.addChild(text); // 箱に入れる

            nodeGroup.visible = false; // 最初は隠しておく
            this.nodeContainer.addChild(nodeGroup); // 世界に追加
            this.nodeSprites.push(nodeGroup); // 配列に保存
        }

        this.fpsText = new PIXI.Text({ text: 'FPS: 0', style: { fontSize: 16, fill: 0x000000 } });
        this.fpsText.x = 10;
        this.fpsText.y = 20;
        this.app.stage.addChild(this.fpsText);

        this.setupEvents();
        
        // ゲームループの登録
        this.app.ticker.add(this.renderLoop);
        
        this.isInitialized = true;
    }

    // ★ イベント設定
    private setupEvents() {
        this.app.stage.eventMode = 'static';
        this.app.stage.hitArea = new PIXI.Rectangle(0, 0, 600, 400);

        this.app.stage.on('pointerdown', (e) => {
            this.isDragging = true;
            this.lastPos = { x: e.global.x, y: e.global.y };
        });
        this.app.stage.on('pointermove', (e) => {
            if (!this.isDragging) return;
            const dx = e.global.x - this.lastPos.x;
            const dy = e.global.y - this.lastPos.y;
            this.world.position.x += dx;
            this.world.position.y += dy;
            this.lastPos = { x: e.global.x, y: e.global.y };
        });
        this.app.stage.on('pointerup', () => (this.isDragging = false));
        this.app.stage.on('pointerupoutside', () => (this.isDragging = false));

        this.app.canvas.addEventListener('wheel', this.onWheel);
    }

    // ★ アロー関数にしておくことで、thisのスコープが外れない＆イベント解除が簡単に！
    private onWheel = (e: WheelEvent) => {
        e.preventDefault();
        const zoomFactor = 1.1;
        const scaleChange = e.deltaY < 0 ? zoomFactor : 1 / zoomFactor;

        const rect = this.app.canvas.getBoundingClientRect();
        const mouseX = e.clientX - rect.left;
        const mouseY = e.clientY - rect.top;

        const oldScale = this.world.scale.x;
        let newScale = oldScale * scaleChange;
        newScale = Math.max(0.05, Math.min(newScale, 10));
        const actualScaleChange = newScale / oldScale;

        this.world.position.x = mouseX - (mouseX - this.world.position.x) * actualScaleChange;
        this.world.position.y = mouseY - (mouseY - this.world.position.y) * actualScaleChange;
        this.world.scale.set(newScale);
    };

    private isVisible(x: number, y: number): boolean {
        const screenX = x * this.world.scale.x + this.world.position.x;
        const screenY = y * this.world.scale.y + this.world.position.y;
        return screenX >= -50 && screenX <= 650 && screenY >= -50 && screenY <= 450;
    }

    // ★ 描画ループ（アロー関数）
    private renderLoop = () => {
        if (this.isDestroyed) return;

        this.engine.step();
        const state = this.engine.getState({});
        const nodeArray = new Float32Array(state.nodes);
        const edgeArray = new Float32Array(state.edges);

        const nodeCount = nodeArray.length / 4;
        while (this.nodeSprites.length < nodeCount) {
            const sprite = new PIXI.Sprite(this.circleTexture!);
            sprite.anchor.set(0.5);
            this.nodeSprites.push(sprite);
            this.nodeContainer.addChild(sprite);
        }

        // ノードの更新とカリング
        let visibleNodeCount = 0;
        let nodeIndex = 0; // 何番目のノードを処理しているか

        for (let i = 0; i < nodeArray.length; i += 4) {
            const x = nodeArray[i];
            const y = nodeArray[i + 1];
            const colorId = nodeArray[i + 3];

            // 100個（用意したコンテナ数）を超えたら処理しない
            if (nodeIndex >= this.nodeSprites.length) break;

            const group = this.nodeSprites[nodeIndex];

            if (this.isVisible(x, y)) {
                group.visible = true;
                group.x = x;
                group.y = y;

                // 状態（colorId）に応じて枠線の色を変更する
                // group.children[0] は、さっき入れた「白抜きの円（Graphics）」のこと
                const bg = group.children[0] as PIXI.Graphics;
                const borderColor = colorId === 0 ? 0x3498db : 0xe74c3c;
                
                // 色を塗り直す
                bg.clear();
                bg.circle(0, 0, 14);
                bg.fill(0xffffff);
                bg.stroke({ width: 3, color: borderColor });

                visibleNodeCount++;
            } else {
                group.visible = false;
            }
            nodeIndex++;
        }

        // 余った（使わなかった）ノードを非表示にする
        for (let i = nodeIndex; i < this.nodeSprites.length; i++) {
            this.nodeSprites[i].visible = false;
        }

        this.edgeGraphics.clear();
        for (let i = 0; i < edgeArray.length; i += 4) {
            const fromIdx = edgeArray[i];
            const toIdx = edgeArray[i + 1];
            const fx = nodeArray[fromIdx * 4];
            const fy = nodeArray[fromIdx * 4 + 1];
            const tx = nodeArray[toIdx * 4];
            const ty = nodeArray[toIdx * 4 + 1];

            if (this.isVisible(fx, fy) || this.isVisible(tx, ty)) {
                this.edgeGraphics.moveTo(fx, fy);
                this.edgeGraphics.lineTo(tx, ty);
            }
        }
        this.edgeGraphics.stroke({ width: 2 / this.world.scale.x, color: 0x999999 });

        this.fpsText.text = `FPS: ${Math.round(this.app.ticker.FPS)} / Visible: ${visibleNodeCount}`;
    };

    // ★ 破棄処理（Reactから呼ばれる）
    public destroy() {
        this.isDestroyed = true;
        if (this.isInitialized) {
            this.app.canvas.removeEventListener('wheel', this.onWheel);
            this.app.ticker.remove(this.renderLoop);
            this.app.stage.destroy({ children: true });
            this.app.destroy({ removeView: true });
        }
    }
}