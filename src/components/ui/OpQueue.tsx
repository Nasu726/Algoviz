import React from 'react';

interface Props {
    ops: string[];
    /** 次に実行する (実行中の) 操作の位置 */
    opIndex: number;
    compact?: boolean;
}

// 操作の並び。実行済み / 次に実行するもの を色で分ける
export const OpQueue: React.FC<Props> = ({ ops, opIndex, compact }) => (
    <div style={{ fontFamily: 'monospace', fontSize: compact ? '13px' : '15px',
                  lineHeight: 1.7 }}>
        {ops.length === 0
            ? <span style={{ color: '#90a4ae' }}>（操作がありません）</span>
            : ops.map((op, i) => (
                <span key={i} style={{
                    // 1操作は塊で折り返す。放っておくと "union 4 5" が "un" と "ion 4 5" に割れる
                    display: 'inline-block', whiteSpace: 'nowrap',
                    marginRight: '10px',
                    color: i < opIndex ? '#90a4ae' : i === opIndex ? '#e74c3c' : '#000',
                    fontWeight: i === opIndex ? 'bold' : 'normal',
                }}>{op}</span>
            ))}
    </div>
);
