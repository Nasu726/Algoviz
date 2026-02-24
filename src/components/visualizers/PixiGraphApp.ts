import * as PIXI from 'pixi.js';

export interface NodeMeta {
    label?: string;
    isStart?: boolean;
    isAccepting?: boolean;
}

export class PixiGraphApp {
    private app: PIXI.Application;
    private container: HTMLDivElement;
    private engine: any;
    
    // PixiJSのオブジェクト群
    private world!: PIXI.Container;
    private edgeGraphics!: PIXI.Graphics;
    private nodeContainer!: PIXI.Container;
    private nodeSprites: PIXI.Container[] = [];
    private edgeWeightTexts: PIXI.Text[] = [];
    private nodeMetadata: NodeMeta[] = [];
    private fpsText!: PIXI.Text;
    private nodeRadius: number = 20.0;
    private isDirected: boolean = false;
    private isAutomaton: boolean = false;
    private showWeights: boolean = false;
    private startNodeIndex: number = -1;
    private acceptingNodeIndices: Set<number> = new Set();
    
    // 状態管理フラグ
    private isInitialized = false;
    private isDestroyed = false;

    // マウス操作用
    private isDragging = false;
    private lastPos = { x: 0, y: 0 };

    constructor(
        container: HTMLDivElement, 
        engine: any, 
    ) {
        this.container = container;
        this.engine = engine;
        this.app = new PIXI.Application();
    }

    // ==========================================
    // 追加: Reactから設定を受け取るメソッド
    // ==========================================
    public updateSettings(settings: {
        isDirected: boolean;
        showWeights: boolean;
        isAutomaton: boolean;
        startNode: string;
        acceptingNodes: string;
    }) {
        this.isDirected = settings.isDirected;
        this.showWeights = settings.showWeights;
        this.isAutomaton = settings.isAutomaton;
        
        // テキストをパースしてインデックス化
        this.startNodeIndex = parseInt(settings.startNode, 10);
        this.acceptingNodeIndices.clear();
        settings.acceptingNodes.split(',').forEach(s => {
            const idx = parseInt(s.trim(), 10);
            if (!isNaN(idx)) this.acceptingNodeIndices.add(idx);
        });
        
        // nodeMetadataを上書き
        this.nodeMetadata = [];
        if (this.isAutomaton) {
            if (!isNaN(this.startNodeIndex)) {
                this.nodeMetadata[this.startNodeIndex] = { ...this.nodeMetadata[this.startNodeIndex], isStart: true };
            }
            this.acceptingNodeIndices.forEach(idx => {
                this.nodeMetadata[idx] = { ...this.nodeMetadata[idx], isAccepting: true };
            });
        }
    }

    // 初期化処理（Reactから呼ばれる）
    public async init() {
        await this.app.init({ 
            width: 800, 
            height: 600, 
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

            // 1. 白抜きの円
            const bg = new PIXI.Graphics();
            nodeGroup.addChild(bg);

            // 2. 二重丸（Accepting）用の輪っか
            const acceptRing = new PIXI.Graphics();
            acceptRing.label = "acceptRing";
            nodeGroup.addChild(acceptRing);

            // 3. "start ->" の矢印
            const startArrow = new PIXI.Graphics();
            startArrow.label = "startArrow";
            const arrowLength = 40;
            const startLineX = -this.nodeRadius - arrowLength;
            const endLineX = -this.nodeRadius - 2;
            startArrow.moveTo(startLineX, 0).lineTo(endLineX, 0);
            startArrow.moveTo(endLineX, 0).lineTo(endLineX - 8, -6);
            startArrow.moveTo(endLineX, 0).lineTo(endLineX - 8, 6);
            startArrow.stroke({ width: 2, color: 0x555555 }); // 白背景で見えるように濃いグレー
            const startText = new PIXI.Text({ text: 'start', style: { fontSize: 12, fill: 0x555555, fontWeight: 'bold' } });
            startText.anchor.set(0.5, 1.0);
            startText.position.set(startLineX + arrowLength / 2, -2);
            startArrow.addChild(startText);
            nodeGroup.addChild(startArrow);

            // 4. ノードのラベルテキスト
            const labelText = new PIXI.Text({ text: '', style: { fontSize: 14, fill: 0x333333, fontWeight: 'bold' } });
            labelText.anchor.set(0.5);
            labelText.label = "labelText";
            nodeGroup.addChild(labelText);

            // 5. ノードの重みテキスト（緑色）
            const weightText = new PIXI.Text({ text: '', style: { fontSize: 13, fill: 0x27ae60, stroke: { color: 0xffffff, width: 3 }, fontWeight: 'bold' } });
            weightText.anchor.set(0.5);
            weightText.label = "weightText";
            nodeGroup.addChild(weightText);

            nodeGroup.visible = false;
            this.nodeContainer.addChild(nodeGroup);
            this.nodeSprites.push(nodeGroup);
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
        this.app.stage.hitArea = new PIXI.Rectangle(0, 0, this.app.canvas.width, this.app.canvas.height);

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
        return screenX >= -50 && screenX <= this.app.canvas.width + 50 && screenY >= -50 && screenY <= this.app.canvas.height + 50;
    }

    // ノードの座標(fx, fy)と、既に使用されている角度の配列から、最大の隙間の角度を返す関数
    private getLargestGapAngle(angles: number[] | undefined, fx: number, fy: number): number {
        let baseAngle = -Math.PI / 2; // デフォルトは真上

        if (angles && angles.length > 0) {
            if (angles.length === 1) {
                // 辺が1本ならその真逆
                baseAngle = angles[0] + Math.PI;
            } else {
                const sortedAngles = angles.map(a => a >= 0 ? a : a + 2 * Math.PI).sort((a, b) => a - b);
                let maxGap = 0;
                
                for (let j = 0; j < sortedAngles.length; j++) {
                    const a1 = sortedAngles[j];
                    const a2 = sortedAngles[(j + 1) % sortedAngles.length];
                    let gap = a2 - a1;
                    if (gap < 0) gap += 2 * Math.PI; 
                    
                    if (gap > maxGap) {
                        maxGap = gap;
                        baseAngle = a1 + gap / 2; // 隙間の中央
                    }
                }
            }
        } else {
            // 辺が全く無い場合はキャンバスの外側へ
            const outX = fx - 400; 
            const outY = fy - 300;
            if (outX !== 0 || outY !== 0) {
                baseAngle = Math.atan2(outY, outX);
            }
        }
        return baseAngle;
    }

    private renderLoop = () => {
        if (this.isDestroyed) return;

        this.engine.step();
        const state = this.engine.getState({});
        const nodeArray = new Float32Array(state.nodes);
        const edgeArray = new Float32Array(state.edges);

        // ==========================================
        // 1. エッジテキストプールの準備
        // ==========================================
        const edgeCount = edgeArray.length / 4;
        while (this.edgeWeightTexts.length < edgeCount) {
            const text = new PIXI.Text({ text: '', style: { fontSize: 13, fill: 0xe74c3c, stroke: { color: 0xffffff, width: 3 }, fontWeight: 'bold' } });
            text.anchor.set(0.5);
            this.edgeWeightTexts.push(text);
            this.world.addChild(text);
        }
        for (let i = edgeCount; i < this.edgeWeightTexts.length; i++) this.edgeWeightTexts[i].visible = false;

        // ==========================================
        // 2. 各ノードに繋がっている辺の角度を収集 ＆ 多重辺のカウント
        // ==========================================
        const nodeAngles: { [nodeIdx: number]: number[] } = {};
        const edgeTotalCounts: { [key: string]: number } = {}; // ★追加: ペア間の辺の総数

        for (let i = 0; i < edgeArray.length; i += 4) {
            const fromIdx = edgeArray[i], toIdx = edgeArray[i + 1];
            
            // ペア間の辺の総数をカウント（A->BもB->Aも同じペアとしてまとめる）
            const minIdx = Math.min(fromIdx, toIdx), maxIdx = Math.max(fromIdx, toIdx);
            const edgeKey = `${minIdx}-${maxIdx}`;
            edgeTotalCounts[edgeKey] = (edgeTotalCounts[edgeKey] || 0) + 1;

            if (fromIdx !== toIdx) { // 自己ループ以外
                const fx = nodeArray[fromIdx * 4], fy = nodeArray[fromIdx * 4 + 1];
                const tx = nodeArray[toIdx * 4], ty = nodeArray[toIdx * 4 + 1];
                
                const angleFrom = Math.atan2(ty - fy, tx - fx);
                const angleTo = Math.atan2(fy - ty, fx - tx);

                if (!nodeAngles[fromIdx]) nodeAngles[fromIdx] = [];
                nodeAngles[fromIdx].push(angleFrom);

                if (!nodeAngles[toIdx]) nodeAngles[toIdx] = [];
                nodeAngles[toIdx].push(angleTo);
            }
        }

        // ==========================================
        // 3. エッジの更新
        // ==========================================
        this.edgeGraphics.clear();
        const edgeCounts: { [key: string]: number } = {};
        const arrowPolygons: number[][] = [];
        const selfLoopBaseAngles: { [nodeIdx: number]: number } = {};

        for (let i = 0; i < edgeArray.length; i += 4) {
            const fromIdx = edgeArray[i], toIdx = edgeArray[i + 1], weight = edgeArray[i + 2];
            const fx = nodeArray[fromIdx * 4], fy = nodeArray[fromIdx * 4 + 1];
            const tx = nodeArray[toIdx * 4], ty = nodeArray[toIdx * 4 + 1];

            if (this.isVisible(fx, fy) || this.isVisible(tx, ty)) {
                const minIdx = Math.min(fromIdx, toIdx), maxIdx = Math.max(fromIdx, toIdx);
                const edgeKey = `${minIdx}-${maxIdx}`;
                const count = edgeCounts[edgeKey] || 0;
                edgeCounts[edgeKey] = count + 1;
                const totalEdges = edgeTotalCounts[edgeKey]; // ★そのペア間に何本の辺があるか

                const actualRadius = this.nodeRadius + 2;
                const textObj = this.edgeWeightTexts[i / 4];
                textObj.visible = this.showWeights;
                textObj.text = weight.toString();

                if (fromIdx === toIdx) {
                    // ----------------------------------------
                    // ★ 自己ループの処理
                    // ----------------------------------------
                    let baseAngle: number;
                    
                    if (selfLoopBaseAngles[fromIdx] !== undefined) {
                        // 2本目以降の自己ループなら、記憶しておいた同じ角度を使う
                        baseAngle = selfLoopBaseAngles[fromIdx];
                    } else {
                        // 1本目の自己ループなら最大の隙間を探して記憶する
                        baseAngle = this.getLargestGapAngle(nodeAngles[fromIdx], fx, fy);
                        selfLoopBaseAngles[fromIdx] = baseAngle;
                        
                        if (!nodeAngles[fromIdx]) nodeAngles[fromIdx] = [];
                        nodeAngles[fromIdx].push(baseAngle);
                    }

                    // countが0（1本目）なら常に基本サイズ。多重ループ（count > 0）の時だけ大きくなる
                    const loopDistance = actualRadius * 3.5 + count * (actualRadius * 1.5); 
                    const spread = Math.PI / 5;
                    const outAngle = baseAngle + spread, inAngle = baseAngle - spread;

                    const cp1X = fx + loopDistance * Math.cos(outAngle), cp1Y = fy + loopDistance * Math.sin(outAngle);
                    const cp2X = fx + loopDistance * Math.cos(inAngle), cp2Y = fy + loopDistance * Math.sin(inAngle);
                    const startX = fx + actualRadius * Math.cos(outAngle), startY = fy + actualRadius * Math.sin(outAngle);
                    const endX = fx + actualRadius * Math.cos(inAngle), endY = fy + actualRadius * Math.sin(inAngle);

                    this.edgeGraphics.moveTo(startX, startY).bezierCurveTo(cp1X, cp1Y, cp2X, cp2Y, endX, endY);
                    
                    // テキストの距離を、実際の曲線の頂点（loopDistanceの約75%）に合わせる
                    const textDist = loopDistance * 0.75 + 10; 
                    textObj.position.set(fx + textDist * Math.cos(baseAngle), fy + textDist * Math.sin(baseAngle));

                    if (this.isDirected) {
                        const dirX = endX - cp2X, dirY = endY - cp2Y;
                        const arrowAngle = Math.atan2(dirY, dirX), arrowSize = 10, wingAngle = Math.PI / 6;
                        arrowPolygons.push([endX, endY, endX - arrowSize * Math.cos(arrowAngle - wingAngle), endY - arrowSize * Math.sin(arrowAngle - wingAngle), endX - arrowSize * Math.cos(arrowAngle + wingAngle), endY - arrowSize * Math.sin(arrowAngle + wingAngle)]);
                    }
                } else {
                    // ----------------------------------------
                    // ★ 通常の辺・多重辺の処理
                    // ----------------------------------------
                    const dx = tx - fx, dy = ty - fy;
                    const dist = Math.sqrt(dx * dx + dy * dy);
                    const midX = (fx + tx) / 2, midY = (fy + ty) / 2;

                    // ★ 変更2：総数に応じて対称的なオフセットを計算する
                    let offset = 0;
                    if (totalEdges > 1) {
                        const spacing = 22; // 曲線の膨らみ幅
                        if (totalEdges % 2 === 1) {
                            // 奇数本 (例: 3本なら 0, +22, -22)
                            if (count > 0) {
                                const pair = Math.ceil(count / 2);
                                offset = (count % 2 === 1 ? 1 : -1) * pair * spacing;
                            }
                        } else {
                            // 偶数本 (例: 2本なら +11, -11)
                            const pair = Math.floor(count / 2);
                            const base = spacing / 2 + pair * spacing;
                            offset = (count % 2 === 0 ? 1 : -1) * base;
                        }
                    }

                    // A->B と B->A で曲がる方向が同じ側にならないように反転処理
                    const isReversed = fromIdx > toIdx;
                    const actualOffset = isReversed ? -offset : offset;

                    if (actualOffset === 0) {
                        // 直線
                        const ndx = dx / dist, ndy = dy / dist;
                        const startX = fx + ndx * actualRadius, startY = fy + ndy * actualRadius;
                        const endX = tx - ndx * actualRadius, endY = ty - ndy * actualRadius;

                        this.edgeGraphics.moveTo(startX, startY).lineTo(endX, endY);

                        // 直線の場合は進行方向の左側に配置
                        const nx = -ndy, ny = ndx;
                        textObj.position.set(midX + nx * 12, midY + ny * 12);

                        if (this.isDirected) {
                            const arrowAngle = Math.atan2(ndy, ndx), arrowSize = 10, wingAngle = Math.PI / 6;
                            arrowPolygons.push([endX, endY, endX - arrowSize * Math.cos(arrowAngle - wingAngle), endY - arrowSize * Math.sin(arrowAngle - wingAngle), endX - arrowSize * Math.cos(arrowAngle + wingAngle), endY - arrowSize * Math.sin(arrowAngle + wingAngle)]);
                        }
                    } else {
                        // 曲線
                        const normalX = -dy / dist, normalY = dx / dist;
                        const controlX = midX + normalX * actualOffset, controlY = midY + normalY * actualOffset;
                        
                        const vFromControlX = controlX - fx, vFromControlY = controlY - fy;
                        const lenFrom = Math.sqrt(vFromControlX ** 2 + vFromControlY ** 2);
                        const startX = fx + (vFromControlX / lenFrom) * actualRadius, startY = fy + (vFromControlY / lenFrom) * actualRadius;

                        const vToControlX = controlX - tx, vToControlY = controlY - ty;
                        const lenTo = Math.sqrt(vToControlX ** 2 + vToControlY ** 2);
                        const endX = tx + (vToControlX / lenTo) * actualRadius, endY = ty + (vToControlY / lenTo) * actualRadius;

                        this.edgeGraphics.moveTo(startX, startY).quadraticCurveTo(controlX, controlY, endX, endY);

                        // 曲線の頂点にテキストを配置
                        const apexX = 0.25 * startX + 0.5 * controlX + 0.25 * endX;
                        const apexY = 0.25 * startY + 0.5 * controlY + 0.25 * endY;
                        const cdx = controlX - midX, cdy = controlY - midY;
                        const clen = Math.sqrt(cdx * cdx + cdy * cdy);
                        const ncx = clen > 0 ? cdx / clen : 0, ncy = clen > 0 ? cdy / clen : 0;
                        
                        // 曲がっている方向の外側に自然に配置される
                        textObj.position.set(apexX + ncx * 12, apexY + ncy * 12);

                        if (this.isDirected) {
                            const dirX = endX - controlX, dirY = endY - controlY;
                            const arrowAngle = Math.atan2(dirY, dirX), arrowSize = 10, wingAngle = Math.PI / 6;
                            arrowPolygons.push([endX, endY, endX - arrowSize * Math.cos(arrowAngle - wingAngle), endY - arrowSize * Math.sin(arrowAngle - wingAngle), endX - arrowSize * Math.cos(arrowAngle + wingAngle), endY - arrowSize * Math.sin(arrowAngle + wingAngle)]);
                        }
                    }
                }
            }
        }
        
        this.edgeGraphics.stroke({ width: 2 / this.world.scale.x, color: 0x999999 });
        for (const poly of arrowPolygons) {
            this.edgeGraphics.poly(poly).fill({ color: 0x999999 });
        }

        // ==========================================
        // 4. ノードの更新 (ここで重みテキストを配置する)
        // ==========================================
        let visibleNodeCount = 0;
        let nodeIndex = 0;

        for (let i = 0; i < nodeArray.length; i += 4) {
            if (nodeIndex >= this.nodeSprites.length) break;

            const x = nodeArray[i], y = nodeArray[i + 1], weight = nodeArray[i + 2], colorId = nodeArray[i + 3];
            const group = this.nodeSprites[nodeIndex];

            if (this.isVisible(x, y)) {
                group.visible = true;
                group.x = x; group.y = y;

                const borderColor = colorId === 0 ? 0x3498db : 0xe74c3c;
                const bg = group.children[0] as PIXI.Graphics;
                bg.clear().circle(0, 0, this.nodeRadius).fill(0xffffff).stroke({ width: 3, color: borderColor });

                const meta = this.nodeMetadata[nodeIndex] || {};
                
                const labelText = group.getChildByLabel("labelText") as PIXI.Text;
                if (labelText) labelText.text = meta.label !== undefined ? meta.label : nodeIndex.toString();

                // Label
                const text = group.getChildByLabel("label") as PIXI.Text;
                if (text) text.text = meta.label || (this.isAutomaton ? `q_${nodeIndex}` : `${nodeIndex}`); // ★ オートマトンならq_、違えば数字のみ

                const acceptRing = group.getChildByLabel("acceptRing") as PIXI.Graphics;
                if (acceptRing) {
                    acceptRing.clear();
                    if (meta.isAccepting) {
                        acceptRing.circle(0, 0, this.nodeRadius - 4).stroke({ width: 2, color: borderColor });
                        acceptRing.visible = true;
                    } else acceptRing.visible = false;
                }

                const startArrow = group.getChildByLabel("startArrow") as PIXI.Graphics;
                if (startArrow) startArrow.visible = !!meta.isStart;

                const wText = group.getChildByLabel("weightText") as PIXI.Text;
                if (wText) {
                    wText.visible = this.showWeights;
                    wText.text = weight.toString();
                    
                    // ★ 自己ループも含めた上で、最も広く空いている角度を再計算！
                    const bestAngle = this.getLargestGapAngle(nodeAngles[nodeIndex], x, y);
                    
                    // その角度に向かって、ノードの半径＋15ピクセルの距離に配置
                    wText.position.set(Math.cos(bestAngle) * (this.nodeRadius + 15), Math.sin(bestAngle) * (this.nodeRadius + 15));
                }
                visibleNodeCount++;
            } else {
                group.visible = false;
            }
            nodeIndex++;
        }

        for (let i = nodeIndex; i < this.nodeSprites.length; i++) this.nodeSprites[i].visible = false;

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