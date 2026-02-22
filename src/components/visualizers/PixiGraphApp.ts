import * as PIXI from 'pixi.js';

export class PixiGraphApp {
    private app: PIXI.Application;
    private container: HTMLDivElement;
    private engine: any;
    
    // PixiJSのオブジェクト群
    private world!: PIXI.Container;
    private edgeGraphics!: PIXI.Graphics;
    private nodeContainer!: PIXI.Container;
    private nodeSprites: PIXI.Sprite[] = [];
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

        const g = new PIXI.Graphics();
        g.circle(0, 0, 10).fill(0xffffff);
        this.circleTexture = this.app.renderer.generateTexture(g);

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

        const nodeCount = nodeArray.length / 3;
        while (this.nodeSprites.length < nodeCount) {
            const sprite = new PIXI.Sprite(this.circleTexture!);
            sprite.anchor.set(0.5);
            this.nodeSprites.push(sprite);
            this.nodeContainer.addChild(sprite);
        }

        let visibleNodeCount = 0;
        for (let i = 0; i < nodeArray.length; i += 3) {
            const x = nodeArray[i];
            const y = nodeArray[i + 1];
            const colorId = nodeArray[i + 2];
            const sprite = this.nodeSprites[i / 3];

            if (this.isVisible(x, y)) {
                sprite.visible = true;
                sprite.x = x;
                sprite.y = y;
                sprite.tint = colorId === 0 ? 0x3498db : 0xe74c3c;
                visibleNodeCount++;
            } else {
                sprite.visible = false;
            }
        }

        this.edgeGraphics.clear();
        for (let i = 0; i < edgeArray.length; i += 3) {
            const fromIdx = edgeArray[i];
            const toIdx = edgeArray[i + 1];
            const fx = nodeArray[fromIdx * 3];
            const fy = nodeArray[fromIdx * 3 + 1];
            const tx = nodeArray[toIdx * 3];
            const ty = nodeArray[toIdx * 3 + 1];

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