"""
tsf_debug_receiver.py
─────────────────────
TSF IME 偵錯接收端

啟動後監聽 TCP 127.0.0.1:9876，等待 C++ DebugSink 連線。
每收到一行 "[TAG] TEXT" 就格式化輸出到終端機。

使用方式：
    python tsf_debug_receiver.py            # 預設 port 9876
    python tsf_debug_receiver.py 9999       # 自訂 port

注意：請在 regsvr32 / 啟動 IME 之前先跑這個腳本。
"""

from __future__ import annotations

import socket
import sys
import threading
from datetime import datetime


# ─────────────────────────────────────────────────────────────
#  TSFDebugReceiver  ── 獨立封裝，與主程式邏輯隔離
# ─────────────────────────────────────────────────────────────
class TSFDebugReceiver:
    """
    單執行緒 TCP 伺服器（一次只接受一個 IME 連線）。
    每筆訊息格式：[TAG] TEXT\\n
    """

    HOST = "127.0.0.1"
    BUFFER = 4096

    # TAG → 終端機 ANSI 顏色代碼
    _COLORS: dict[str, str] = {
        "IME":    "\033[96m",   # 青色  — 生命週期事件
        "KEY":    "\033[92m",   # 綠色  — 目前組字緩衝
        "COMMIT": "\033[93m",   # 黃色  — 確認送出
        "CANCEL": "\033[91m",   # 紅色  — 取消組字
    }
    _RESET = "\033[0m"

    def __init__(self, port: int = 9876) -> None:
        self._port = port
        self._server_sock: socket.socket | None = None
        self._stop_event = threading.Event()

    # ── 公開 API ─────────────────────────────────────────────

    def start(self) -> None:
        """啟動伺服器（阻塞直到 Ctrl-C）。"""
        self._server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._server_sock.bind((self.HOST, self._port))
        self._server_sock.listen(1)
        self._server_sock.settimeout(1.0)   # 讓 accept() 可被中斷

        print(f"[TSFDebugReceiver] 監聽 {self.HOST}:{self._port}  (按 Ctrl-C 結束)")
        print("─" * 60)

        try:
            while not self._stop_event.is_set():
                try:
                    conn, addr = self._server_sock.accept()
                except socket.timeout:
                    continue

                print(f"\n[TSFDebugReceiver] IME 已連線：{addr[0]}:{addr[1]}")
                print("─" * 60)
                self._handle_connection(conn)
                print("─" * 60)
                print("[TSFDebugReceiver] IME 已斷線，等待下一次連線…\n")

        except KeyboardInterrupt:
            print("\n[TSFDebugReceiver] 收到 Ctrl-C，正在關閉…")
        finally:
            self._server_sock.close()

    def stop(self) -> None:
        """從其他執行緒停止伺服器。"""
        self._stop_event.set()

    # ── 私有方法 ─────────────────────────────────────────────

    def _handle_connection(self, conn: socket.socket) -> None:
        """持續從同一條連線讀取訊息，直到 IME 斷線。"""
        buffer = ""
        with conn:
            while True:
                try:
                    chunk = conn.recv(self.BUFFER)
                except OSError:
                    break
                if not chunk:
                    break

                buffer += chunk.decode("utf-8", errors="replace")

                # 按換行切分訊息
                while "\n" in buffer:
                    line, buffer = buffer.split("\n", 1)
                    line = line.strip()
                    if line:
                        self._print_message(line)

    def _print_message(self, line: str) -> None:
        """格式化並印出一筆訊息。"""
        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]

        # 解析 "[TAG] TEXT"
        tag = "???"
        text = line
        if line.startswith("["):
            end = line.find("]")
            if end != -1:
                tag  = line[1:end]
                text = line[end + 2:]   # 跳過 "] "

        color = self._COLORS.get(tag, "\033[97m")   # 未知 TAG → 白色
        print(f"{timestamp}  {color}[{tag:6s}]{self._RESET}  {text}")


# ─────────────────────────────────────────────────────────────
#  Entry point
# ─────────────────────────────────────────────────────────────
if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9876
    TSFDebugReceiver(port=port).start()
