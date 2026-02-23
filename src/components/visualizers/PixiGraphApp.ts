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
    private nodeRadius = 20.0;
    private isDirected: boolean;
    // private isAutomaton: boolean;
    
    // 状態管理フラグ
    private isInitialized = false;
    private isDestroyed = false;

    // マウス操作用
    private isDragging = false;
    private lastPos = { x: 0, y: 0 };

    constructor(
        container: HTMLDivElement, 
        engine: any, 
        isDirected: boolean = false, 
        // isAutomaton: boolean = false
    ) {
        this.container = container;
        this.engine = engine;
        this.isDirected = isDirected;
        // this.isAutomaton = isAutomaton;
        this.app = new PIXI.Application();
    }

    // 初期化処理（Reactから呼ばれる）
    public async init() {
        await this.app.init({ 
            width: 600, 
            height: 400, 
            backgroundColor: 0xfcfcfc,
            antialias: true,
            resolution: window.devicePixelRatio || 1,
            autoDensity: true
        });
        
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
            bg.circle(0, 0, this.nodeRadius);
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
                bg.circle(0, 0, this.nodeRadius);
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
        // ★ 追加：辺の出現回数を記録する辞書（Map）
        const edgeCounts: { [key: string]: number } = {};

        for (let i = 0; i < edgeArray.length; i += 4) {
            const fromIdx = edgeArray[i];
            const toIdx = edgeArray[i + 1];
            const fx = nodeArray[fromIdx * 4];
            const fy = nodeArray[fromIdx * 4 + 1];
            const tx = nodeArray[toIdx * 4];
            const ty = nodeArray[toIdx * 4 + 1];

            if (this.isVisible(fx, fy) || this.isVisible(tx, ty)) {
                
                // ★ 何回目の辺かをカウントする処理
                // 向きが違っても（A→BとB→A）同じペアとして数えるため、小さい方を前にする
                const minIdx = Math.min(fromIdx, toIdx);
                const maxIdx = Math.max(fromIdx, toIdx);
                const edgeKey = `${minIdx}-${maxIdx}`;
                
                if (edgeCounts[edgeKey] === undefined) {
                    edgeCounts[edgeKey] = 0;
                }
                const count = edgeCounts[edgeKey];
                edgeCounts[edgeKey]++; // 次回のためにカウントアップ

                if (fromIdx === toIdx) {
                    // 【自己ループの場合】
                    // カウントが増えるごとに、円の半径と位置を大きくしていく（同心円のように広がる）
                    const radius = 15 + count * 10; // 15, 25, 35...と大きくなる
                    this.edgeGraphics.circle(fx + radius + 5, fy - radius - 5, radius);
                } else {
                    // 【多重辺の場合】
                    const midX = (fx + tx) / 2;
                    const midY = (fy + ty) / 2;
                    const dx = tx - fx;
                    const dy = ty - fy;
                    const normalX = -dy;
                    const normalY = dx;
                    const length = Math.sqrt(normalX * normalX + normalY * normalY);
                    
                    // カウントに応じて膨らみ方（offset）を変える
                    let offset = 0;
                    if (count > 0) {
                        const direction = count % 2 === 1 ? 1 : -1; // 奇数ならプラス、偶数ならマイナス
                        const magnitude = Math.ceil(count / 2) * 20; // 20, 20, 40, 40...
                        offset = direction * magnitude;
                    }

                    const controlX = midX + (normalX / length) * offset;
                    const controlY = midY + (normalY / length) * offset;

                    this.edgeGraphics.moveTo(fx, fy);
                    let dirX = 0;
                    let dirY = 0;

                    if (offset === 0) {
                        // 1本目（offset=0）は単なる直線にする
                        this.edgeGraphics.lineTo(tx, ty);
                        const len = Math.sqrt((tx - fx) ** 2 + (ty - fy) ** 2);
                        dirX = (tx - fx) / len;
                        dirY = (ty - fy) / len;
                    } else {
                        // 2本目以降はベジェ曲線で膨らませる
                        this.edgeGraphics.quadraticCurveTo(controlX, controlY, tx, ty);
                        const len = Math.sqrt((tx - controlX) ** 2 + (ty - controlY) ** 2);
                        dirX = (tx - controlX) / len;
                        dirY = (ty - controlY) / len;
                    }
                    // 有向グラフなら矢印を描画する
                    if (this.isDirected) {
                        const nodeRadius = 24; // ノードの円の半径（大きくしたサイズに合わせる）
                        const arrowSize = 10;  // 矢印の羽の長さ
                        const angle = Math.PI / 6; // 矢印の羽の角度（30度）

                        // 矢印の先端の位置（ノードの縁に接するように、半径分だけ手前に引く）
                        const tipX = tx - dirX * nodeRadius;
                        const tipY = ty - dirY * nodeRadius;

                        // 進行方向の角度
                        const baseAngle = Math.atan2(dirY, dirX);

                        // 矢印の左羽と右羽の座標を計算
                        const leftX = tipX - arrowSize * Math.cos(baseAngle - angle);
                        const leftY = tipY - arrowSize * Math.sin(baseAngle - angle);
                        const rightX = tipX - arrowSize * Math.cos(baseAngle + angle);
                        const rightY = tipY - arrowSize * Math.sin(baseAngle + angle);

                        // 矢印のV字を描画
                        this.edgeGraphics.moveTo(tipX, tipY);
                        this.edgeGraphics.lineTo(leftX, leftY);
                        this.edgeGraphics.moveTo(tipX, tipY);
                        this.edgeGraphics.lineTo(rightX, rightY);
                    }
                }
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