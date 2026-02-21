
const controlChars = [
    "NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL",
    "BS",  "HT",  "LF",  "VT",  "FF",  "CR",  "SO",  "SI",
    "DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
    "CAN", "EM",  "SUB", "ESC", "FS",  "GS",  "RS",  "US",
    "SP"
]

export const char = (n: number) => {
    if (0<= n && n <= 32){
        return controlChars[n];
    } else if (n===127){
        return "DEL";
    } else {
        return String.fromCharCode(n);
    }
}
