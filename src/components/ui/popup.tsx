import React from "react";

interface PopupProps {
    title: string;
    isOpen: boolean;
    onClose: () => void;
    children: React.ReactNode;
}

const overlayStyle: React.CSSProperties = {
  position: "fixed",
  top: 0,
  left: 0,
  width: "100vw",
  height: "100vh",
  backgroundColor: "rgba(0,0,0,0.4)",
  display: "flex",
  justifyContent: "center",
  alignItems: "center",
  zIndex: 1000
};

const modalStyle: React.CSSProperties = {
  background: "#fff",
  color: '#000000',
  padding: "30px",
  borderRadius: "8px",
  minWidth: "300px",
  position: "relative",
  boxShadow: "0 10px 30px rgba(0,0,0,0.2)"
};

const closeStyle: React.CSSProperties = {
  position: "absolute",
  top: "10px",
  right: "10px",
  border: "1px solid",
  background: "transparent",
  fontSize: "18px",
  cursor: "pointer"
};

export const Popup: React.FC<PopupProps> = ({ title, isOpen, onClose, children }) => {
    if (!isOpen) return null;

    return (
        <div style={overlayStyle} onClick={onClose}>
            <div style={modalStyle} onClick={(e) => e.stopPropagation()}>
                <h2>{title}</h2>
                <button style={closeStyle} onClick={onClose}>閉じる</button>
                <div style={{
                    maxHeight: "60vh",
                    overflowY: "auto",
                    paddingRight: "10px"
                }}>
                    {children}
                </div>
            </div>
        </div>
    );
};
