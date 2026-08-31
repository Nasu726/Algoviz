import React from 'react';
import { Popup } from './popup';

interface Props {
    /** ページの題。ヘルプの見出しにも使う */
    title: string;
    /** 狭い幅。文字を小さくする */
    compact?: boolean;
    onBack: () => void;
    /** 戻るときの確認文。ページによって失うものが違う */
    backConfirm: string;
    /**
     * ヘルプの開閉はページ側が持つ。
     * ショートカットの「ヘルプが開いていたら効かせない」ガードがページ側にあるため。
     */
    isHelpOpen: boolean;
    setIsHelpOpen: (open: boolean) => void;
    help: React.ReactNode;
    children: React.ReactNode;
}

const page: React.CSSProperties = {
    display: 'flex', flexDirection: 'column',
    height: '100vh', width: '100vw',
    margin: 0, overflow: 'hidden', fontFamily: 'sans-serif',
    backgroundColor: '#fff', color: '#000',
};

// どのビジュアライザにも共通の外枠。ヘッダ (戻る / 題 / ヘルプ) とヘルプの窓を持つ。
// 中身は children に任せるので、テープでもキャンバスでも同じ枠に収まる。
export const VisualizerShell: React.FC<Props> = ({
    title, compact, onBack, backConfirm, isHelpOpen, setIsHelpOpen, help, children,
}) => {
    const backToMenu = () => {
        if (window.confirm(backConfirm)) onBack();
    };

    const buttonFont = compact ? '12px' : '16px';

    return (
        <div style={page}>
            <div style={{
                display: 'flex', justifyContent: 'space-between', alignItems: 'center',
                padding: compact ? '8px 10px' : '10px 20px',
                backgroundColor: '#263238', color: 'white', flexShrink: 0,
            }}>
                <button onClick={backToMenu} style={{ cursor: 'pointer', fontSize: buttonFont }}>
                    ◀ 戻る
                </button>
                {/* 題が長いと狭い端末でボタンにぶつかるので、先に縮む側にしておく */}
                <h2 style={{
                    margin: 0, fontSize: compact ? '14px' : '18px',
                    minWidth: 0, padding: '0 6px',
                    overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap',
                }}>
                    {title}
                </h2>
                <button
                    onClick={() => setIsHelpOpen(true)}
                    style={{ cursor: 'pointer', fontWeight: 'bold', fontSize: buttonFont }}
                >
                    ヘルプ ❓
                </button>
            </div>

            {children}

            <Popup title={`${title} のヘルプ`} isOpen={isHelpOpen} onClose={() => setIsHelpOpen(false)}>
                {help}
            </Popup>
        </div>
    );
};
