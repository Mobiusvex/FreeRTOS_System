import sys
import os
import serial
import serial.tools.list_ports
from PyQt6.QtWidgets import (QApplication, QWidget, QPushButton, QVBoxLayout, 
                             QHBoxLayout, QComboBox, QLineEdit, QTextEdit, 
                             QFileDialog, QLabel, QTabWidget)
from PyQt6.QtCore import QThread, pyqtSignal
import struct
import binascii
import time
import wave

# ============================================================
#  全局常量
# ============================================================
TIMEOUT_DATA_PKT = 0.5      # 数据包 / 起始包超时
TIMEOUT_END_PKT  = 30.0     # 结束包超时（STM32 要算全片 CRC）

KEY_STREAM = bytes([
    0x8B, 0x47, 0x3F, 0x36, 0x3F, 0xD2, 0x95, 0x1D, 0xC4, 0xBF, 0x7B, 0xF2, 0x21, 0x9B, 0xF1, 0x4D,
    0x09, 0x7E, 0xED, 0xF6, 0xD7, 0xDC, 0x7D, 0x01, 0xEF, 0xCA, 0x06, 0x3C, 0xD3, 0x4B, 0x40, 0x6B,
    0xC9, 0xA7, 0x91, 0xBE, 0x52, 0x67, 0xB2, 0x3C, 0x4D, 0x23, 0x5E, 0x2B, 0xBC, 0xC4, 0x1F, 0x85,
    0x24, 0xCC, 0x72, 0x71, 0x01, 0x43, 0xB7, 0x6F, 0x26, 0x62, 0xEC, 0xB1, 0x20, 0xA3, 0x70, 0x8C,
    0x52, 0x02, 0x78, 0x83, 0x58, 0x3E, 0xCF, 0xE2, 0xB2, 0x88, 0x43, 0x17, 0x71, 0x29, 0xF0, 0x09,
    0xB4, 0x77, 0x0B, 0xCE, 0x6B, 0x27, 0x2A, 0x2A, 0x2E, 0xCD, 0x1D, 0xA0, 0xD5, 0x98, 0x22, 0x3C,
    0x85, 0xA6, 0x57, 0x7C, 0xC6, 0x3A, 0xB8, 0xE6, 0xD7, 0xC5, 0xFE, 0x26, 0x3F, 0x47, 0xC6, 0x5F,
    0x66, 0x9F, 0xA8, 0x2B, 0x2F, 0x95, 0xF8, 0x61, 0x6F, 0xF4, 0xA9, 0x16, 0xA6, 0x9E, 0x35, 0x16,
    0x79, 0xC5, 0xA3, 0xF8, 0xED, 0x8B, 0x13, 0x83, 0x95, 0x89, 0x37, 0x0F, 0xF3, 0x81, 0x6D, 0xA8,
    0xB7, 0xD2, 0x76, 0xE4, 0xF4, 0xC2, 0xE4, 0x71, 0xF0, 0xF1, 0x59, 0x0D, 0x34, 0x4A, 0xC5, 0x3F,
    0x1E, 0xFF, 0x24, 0xDB, 0xA4, 0xD1, 0x0C, 0x06, 0x87, 0xFB, 0xDC, 0x3B, 0x5E, 0xAF, 0xE1, 0xFB,
    0x44, 0x36, 0x67, 0x5E, 0x14, 0xC3, 0x2B, 0xF4, 0xE8, 0x9D, 0x4E, 0xFA, 0xE3, 0x63, 0x5F, 0xA1,
    0x1E, 0x29, 0xD5, 0x90, 0xD1, 0x2F, 0xA5, 0x73, 0x09, 0xCC, 0xD1, 0x2B, 0x4E, 0x2C, 0x09, 0x46,
    0x32, 0x6E, 0xB8, 0x1D, 0x68, 0x1C, 0x43, 0x2E, 0x2D, 0xEB, 0x4A, 0x5C, 0xFB, 0xB2, 0x8A, 0x4B,
    0xEA, 0x1F, 0xB3, 0x56, 0x4E, 0x1B, 0x95, 0x85, 0x2C, 0x37, 0x1A, 0xC9, 0xAC, 0x86, 0x1C, 0x07,
])


# ============================================================
#  通用传输线程
# ============================================================
class TransferWorker(QThread):
    log_signal = pyqtSignal(str)

    CMD_START = 0x11
    CMD_DATA  = 0x12
    CMD_END   = 0x14
    CMD_REPLY = 0x13
    ENCRYPT   = True

    def __init__(self, port, baudrate, filepath):
        super().__init__()
        self.port = port
        self.baudrate = baudrate
        self.filepath = filepath

    # ---------------- 工具函数 ----------------
    @staticmethod
    def _elapsed_str(t_start: float) -> str:
        elapsed = time.time() - t_start
        if elapsed < 60:
            return f"{elapsed:.2f} 秒"
        elif elapsed < 3600:
            minutes = int(elapsed // 60)
            seconds = elapsed - minutes * 60
            return f"{minutes} 分 {seconds:.1f} 秒"
        else:
            hours = int(elapsed // 3600)
            minutes = int((elapsed - hours * 3600) // 60)
            seconds = elapsed - hours * 3600 - minutes * 60
            return f"{hours} 小时 {minutes} 分 {seconds:.1f} 秒"

    @staticmethod
    def calc_crc16(data: bytes) -> bytes:
        crc32 = binascii.crc32(data) & 0xffffffff
        return struct.pack('<H', crc32 & 0xffff)

    # ---------------- 子类扩展点 ----------------
    def start_seq(self) -> int:
        """起始包序号，OTA 用 0，音频用存储位置 1~5"""
        return 0

    def build_start_data(self, total_packets: int, original_size: int) -> bytes:
        """起始包数据域（OTA：总包数 2B + 总字节数 4B）"""
        return struct.pack('<H', total_packets) + struct.pack('<I', original_size)

    def read_file(self) -> bytes:
        """
        读取待传输的文件数据。
        默认实现：原样读取。
        子类可以覆盖，做格式转换（如去 WAV 头）。
        """
        with open(self.filepath, 'rb') as f:
            return f.read()
    # ---------------- 帧收发 ----------------
    def send_frame_and_wait_ack(self, ser: serial.Serial, frame: bytes,
                                expected_seq: int, timeout_s=2) -> bool:
        ser.reset_input_buffer()
        ser.write(frame)
        ser.flush()

        reply = b''
        t_end = time.time() + timeout_s
        while len(reply) < 9 and time.time() < t_end:
            chunk = ser.read(9 - len(reply))
            if chunk:
                reply += chunk

        hex_frame = ' '.join(f'{b:02X}' for b in reply)
        self.log_signal.emit(f"(接收数据内容: {hex_frame})")

        if len(reply) != 9:
            self.log_signal.emit(f"⚠️ 回复长度错误，期望9字节，收到{len(reply)}")
            return False
        if reply[0] != 0x68 or reply[-1] != 0x86 or reply[3] != self.CMD_REPLY:
            self.log_signal.emit(f"⚠️ 回复格式错误: {reply.hex()}")
            return False
        recv_seq = struct.unpack('<H', reply[1:3])[0]
        if recv_seq != expected_seq:
            self.log_signal.emit(f"⚠️ 序号不匹配，期望{expected_seq}，收到{recv_seq}")
            return False
        if reply[5] == 0x66:
            return True
        else:
            self.log_signal.emit(f"❌ STM32回复失败(0x{reply[5]:02X})")
            return False

    # ---------------- 主流程 ----------------
    def run(self):
        t_start = time.time()
        ser = None
        try:
            t_open = time.time()
            ser = serial.Serial(self.port, self.baudrate, timeout=3)
            self.log_signal.emit(f"✅ 打开串口 {self.port} 成功 "
                                 f"(耗时 {self._elapsed_str(t_open)})")

           
            raw = self.read_file()
            original_size = len(raw)
            if original_size == 0:
                self.log_signal.emit("❌ 文件为空！")
                return

            self.log_signal.emit(f"📦 文件大小: {original_size} 字节")
            if self.ENCRYPT:
                self.log_signal.emit("🔒 加密方式: KEY_STREAM 异或加密")
            else:
                self.log_signal.emit("📂 加密方式: 不加密（明文传输）")

            # 补齐到 240 整数倍
            MAX_DATA = 240
            pad_len = (MAX_DATA - (len(raw) % MAX_DATA)) % MAX_DATA
            if pad_len > 0:
                raw += b'\xFF' * pad_len
                self.log_signal.emit(f"📏 补齐 {pad_len} 字节 0xFF 到 240 整数倍")

            total_size = len(raw)
            total_crc32 = binascii.crc32(raw) & 0xffffffff
            total_packets = total_size // MAX_DATA
            self.log_signal.emit(f"🔑 总CRC32: {total_crc32:08X}")
            self.log_signal.emit(f"📨 将分 {total_packets} 包发送 (每包{MAX_DATA}字节)")

            # ========== 起始包 ==========
            seq_start = self.start_seq()
            start_data = self.build_start_data(total_packets, original_size)
            start_header = (struct.pack('<B', 0x68)
                            + struct.pack('<H', seq_start)
                            + struct.pack('<B', self.CMD_START)
                            + struct.pack('<B', len(start_data)))
            start_frame_no_crc = start_header + start_data
            start_frame = (start_frame_no_crc
                           + self.calc_crc16(start_frame_no_crc)
                           + struct.pack('<B', 0x86))

            self.log_signal.emit(f"📤 发送起始包 (序号={seq_start})...")
            hex_start = ' '.join(f'{b:02X}' for b in start_frame)
            self.log_signal.emit(f"(发送数据内容: {hex_start})")

            t_start_pkt = time.time()
            ok = self.send_frame_and_wait_ack(ser, start_frame,
                                              expected_seq=seq_start,
                                              timeout_s=TIMEOUT_DATA_PKT)
            if not ok:
                self.log_signal.emit(f"❌ 起始包确认失败 (耗时 {self._elapsed_str(t_start_pkt)})，传输终止")
                self.log_signal.emit(f"⏱️ 总耗时: {self._elapsed_str(t_start)}")
                return
            self.log_signal.emit(f"✅ 起始包确认成功 (耗时 {self._elapsed_str(t_start_pkt)})")

            # ========== 数据包 ==========
            t_data_start = time.time()
            for packet_idx in range(total_packets):
                offset = packet_idx * MAX_DATA
                chunk = raw[offset: offset + MAX_DATA]
                chunk_len = len(chunk)

                if self.ENCRYPT:
                    payload = bytes([
                        b ^ KEY_STREAM[(offset + j) % len(KEY_STREAM)]
                        for j, b in enumerate(chunk)
                    ])
                else:
                    payload = chunk

                header = (struct.pack('<B', 0x68)
                          + struct.pack('<H', packet_idx)
                          + struct.pack('<B', self.CMD_DATA)
                          + struct.pack('<B', chunk_len))
                frame_no_crc = header + payload
                data_frame = (frame_no_crc
                              + self.calc_crc16(frame_no_crc)
                              + struct.pack('<B', 0x86))

                hex_preview = ' '.join(f'{b:02X}' for b in data_frame[:16])
                hex_endview = ' '.join(f'{b:02X}' for b in data_frame[-16:])
                self.log_signal.emit(f"(发送数据前16字节: {hex_preview}...{hex_endview})")

                success = False
                for retry in range(3):
                    self.log_signal.emit(f"📤 发送第 {packet_idx+1}/{total_packets} 包 (重试{retry})")
                    ok = self.send_frame_and_wait_ack(ser, data_frame,
                                                      expected_seq=packet_idx,
                                                      timeout_s=TIMEOUT_DATA_PKT)
                    if ok:
                        self.log_signal.emit(f"✅ 第 {packet_idx+1} 包成功")
                        success = True
                        break
                    else:
                        self.log_signal.emit(f"⚠️ 第 {packet_idx+1} 包失败，重试 {retry+1}/3")
                        time.sleep(0.1)

                if not success:
                    self.log_signal.emit(f"❌ 第 {packet_idx+1} 包发送失败，传输终止")
                    self.log_signal.emit(f"⏱️ 已发送 {packet_idx}/{total_packets} 包，"
                                         f"数据阶段耗时 {self._elapsed_str(t_data_start)}")
                    self.log_signal.emit(f"⏱️ 总耗时: {self._elapsed_str(t_start)}")
                    return

                if (packet_idx + 1) % 100 == 0:
                    elapsed_data = time.time() - t_data_start
                    speed = (packet_idx + 1) / elapsed_data if elapsed_data > 0 else 0
                    remain = (total_packets - packet_idx - 1) / speed if speed > 0 else 0
                    self.log_signal.emit(
                        f"📊 进度 {packet_idx+1}/{total_packets} "
                        f"({(packet_idx+1)*100//total_packets}%)  "
                        f"速度 {speed:.1f} 包/秒  "
                        f"预计剩余 {remain:.0f} 秒"
                    )

            self.log_signal.emit(f"✅ 所有数据包发送完毕 (耗时 {self._elapsed_str(t_data_start)})")

            # ========== 结束包 ==========
            end_header = (struct.pack('<B', 0x68)
                          + struct.pack('<H', total_packets)
                          + struct.pack('<B', self.CMD_END)
                          + struct.pack('<B', 4))
            end_data = struct.pack('<I', total_crc32)
            end_frame_no_crc = end_header + end_data
            end_frame = (end_frame_no_crc
                         + self.calc_crc16(end_frame_no_crc)
                         + struct.pack('<B', 0x86))

            self.log_signal.emit("📤 发送结束包...")
            hex_end = ' '.join(f'{b:02X}' for b in end_frame)
            self.log_signal.emit(f"(发送数据内容: {hex_end})")

            t_end_pkt = time.time()
            ok = self.send_frame_and_wait_ack(ser, end_frame,
                                              expected_seq=total_packets,
                                              timeout_s=TIMEOUT_END_PKT)

            total_elapsed = time.time() - t_start
            if ok:
                self.log_signal.emit("🎉 传输全部完成！总校验通过！")
                self.log_signal.emit(f"⏱️ 结束包校验耗时: {self._elapsed_str(t_end_pkt)}")
                self.log_signal.emit(f"⏱️ 数据阶段耗时: {self._elapsed_str(t_data_start)}")
                self.log_signal.emit(f"⭐ 总耗时: {self._elapsed_str(t_start)}")
                if total_elapsed > 0:
                    speed_kbps = original_size / 1024 / total_elapsed
                    self.log_signal.emit(f"⚡ 平均速率: {speed_kbps:.2f} KB/s")
            else:
                self.log_signal.emit("❌ 结束包确认失败，传输可能不完整！")
                self.log_signal.emit(f"⏱️ 总耗时: {self._elapsed_str(t_start)}")

        except Exception as e:
            self.log_signal.emit(f"💥 发生异常: {e}")
        finally:
            if ser and ser.is_open:
                ser.close()
                self.log_signal.emit("🔌 串口已关闭")


# ============================================================
#  OTA 固件升级线程：0x11 / 0x12 / 0x14，加密
# ============================================================
class OTAWorker(TransferWorker):
    CMD_START = 0x11
    CMD_DATA  = 0x12
    CMD_REPLY = 0X13
    CMD_END   = 0x14
    ENCRYPT   = True

    def start_seq(self):
        return 0

    def build_start_data(self, total_packets, original_size):
        # 总包数(2B) + 总字节数(4B)
        return struct.pack('<H', total_packets) + struct.pack('<I', original_size)


# ============================================================
#  音频下载线程：0x21 / 0x22 / 0x24，明文
#  起始包数据：总包数(2B) + 总字节数(4B) + 文件名(含 '\0')
#  序号：1~5（存储位置）
# ============================================================
class AudioWorker(TransferWorker):
    CMD_START = 0x21
    CMD_DATA  = 0x22
    CMD_REPLY = 0X23
    CMD_END   = 0x24
    ENCRYPT   = False

    NAME_MAX = 19   # 文件名最多 19 字节（不含 '\0'）

    def __init__(self, port, baudrate, filepath, audio_name: str, audio_index: int):
        super().__init__(port, baudrate, filepath)
        self.audio_name = audio_name
        self.audio_index = audio_index

    def start_seq(self):
        return self.audio_index
    # def read_file(self) -> bytes:
    #         """WAV 文件去掉头部，只返回 PCM 数据"""
    #         if self.filepath.lower().endswith('.wav'):
    #             with wave.open(self.filepath, 'rb') as wf:
    #                 return wf.readframes(wf.getnframes())
    #         # 非 WAV 文件，按原样读取
    #         return super().read_file()
    def build_start_data(self, total_packets, original_size):
        name_only = os.path.splitext(self.audio_name)[0]
        name = name_only.encode('utf-8')[:self.NAME_MAX] or b'_'
        name += b'\0'
        data = (struct.pack('<H', total_packets)
                + struct.pack('<I', original_size)
                + name)
        self.log_signal.emit(f"📁 文件名: {self.audio_name!r} -> {len(name)} 字节（含 '\\0'）")
        self.log_signal.emit(f"📍 存储位置: 第 {self.audio_index} 段")
        self.log_signal.emit(f"📐 数据长度字段: {len(data)} (0x{len(data):02X})")
        return data


# ============================================================
#  调试播放线程：0x51，1 字节段号
# ============================================================
class DebugWorker(QThread):
    log_signal = pyqtSignal(str)

    CMD_PLAY = 0x51

    def __init__(self, port, baudrate, audio_index):
        super().__init__()
        self.port = port
        self.baudrate = baudrate
        self.audio_index = audio_index

    @staticmethod
    def calc_crc16(data: bytes) -> bytes:
        return struct.pack('<H', binascii.crc32(data) & 0xffff)

    def run(self):
        ser = None
        try:
            ser = serial.Serial(self.port, self.baudrate, timeout=2)
            self.log_signal.emit(f"🎵 调试: 请求播放第 {self.audio_index} 段")

            header = (struct.pack('<B', 0x68)
                      + struct.pack('<H', 0)
                      + struct.pack('<B', self.CMD_PLAY)
                      + struct.pack('<B', 1))
            payload = struct.pack('<B', self.audio_index)
            frame_no_crc = header + payload
            frame = frame_no_crc + self.calc_crc16(frame_no_crc) + struct.pack('<B', 0x86)

            hex_frame = ' '.join(f'{b:02X}' for b in frame)
            self.log_signal.emit(f"(发送数据内容: {hex_frame})")

            ser.reset_input_buffer()
            ser.write(frame)
            ser.flush()

            reply = b''
            t_end = time.time() + 2
            while len(reply) < 9 and time.time() < t_end:
                chunk = ser.read(9 - len(reply))
                if chunk:
                    reply += chunk

            hex_reply = ' '.join(f'{b:02X}' for b in reply)
            self.log_signal.emit(f"(接收数据内容: {hex_reply})")

            if len(reply) == 9 and reply[0] == 0x68 and reply[-1] == 0x86:
                if reply[5] == 0x66:
                    self.log_signal.emit(f"✅ 播放第 {self.audio_index} 段命令成功")
                else:
                    self.log_signal.emit(f"❌ STM32回复失败 (0x{reply[5]:02X})")
            else:
                self.log_signal.emit(f"⚠️ 响应异常，长度 {len(reply)}")
        except Exception as e:
            self.log_signal.emit(f"💥 调试异常: {e}")
        finally:
            if ser and ser.is_open:
                ser.close()


# ============================================================
#  通用页面
# ============================================================
class TransferPage(QWidget):
    def __init__(self, worker_class,
                 file_dialog_title="选择文件",
                 file_filter="BIN Files (*.bin);;All Files (*)",
                 start_btn_text="开始传输"):
        super().__init__()
        self.worker_class = worker_class
        self.file_dialog_title = file_dialog_title
        self.file_filter = file_filter

        main_layout = QVBoxLayout()

        # 串口选择行
        port_layout = QHBoxLayout()
        port_layout.addWidget(QLabel("串口:"))
        self.port_combo = QComboBox()
        port_layout.addWidget(self.port_combo)

        self.baud_combo = QComboBox()
        self.baud_combo.addItems(["115200", "921600", "460800"])
        port_layout.addWidget(QLabel("波特率:"))
        port_layout.addWidget(self.baud_combo)

        self.refresh_btn = QPushButton("刷新")
        self.refresh_btn.clicked.connect(self.refresh_ports)
        port_layout.addWidget(self.refresh_btn)

        main_layout.addLayout(port_layout)

        # 文件选择行
        file_layout = QHBoxLayout()
        self.file_edit = QLineEdit()
        self.file_edit.setPlaceholderText("请选择文件")
        self.file_btn = QPushButton("浏览")
        self.file_btn.clicked.connect(self.select_file)
        file_layout.addWidget(self.file_edit)
        file_layout.addWidget(self.file_btn)
        main_layout.addLayout(file_layout)

        # 子类扩展控件
        self._build_extra_controls(main_layout)

        # 开始按钮
        self.send_btn = QPushButton(start_btn_text)
        self.send_btn.clicked.connect(self.start_transfer)
        main_layout.addWidget(self.send_btn)

        # 日志
        self.log_text = QTextEdit()
        self.log_text.setReadOnly(True)
        main_layout.addWidget(self.log_text)

        self.setLayout(main_layout)
        self.worker = None
        self.refresh_ports()

    def _build_extra_controls(self, layout):
        pass

    def build_worker(self, port, baud, filepath):
        return self.worker_class(port, baud, filepath)

    def refresh_ports(self):
        self.port_combo.clear()
        for port in serial.tools.list_ports.comports():
            self.port_combo.addItem(port.device)

    def select_file(self):
        file_path, _ = QFileDialog.getOpenFileName(
            self, self.file_dialog_title, "", self.file_filter)
        if file_path:
            self.file_edit.setText(file_path)

    def start_transfer(self):
        if not self.file_edit.text():
            self.log_text.append("请先选择文件")
            return
        if self.worker and self.worker.isRunning():
            self.log_text.append("传输正在进行中...")
            return

        port = self.port_combo.currentText()
        if not port:
            self.log_text.append("未检测到可用串口，请检查设备连接")
            return

        baud = int(self.baud_combo.currentText())
        filepath = self.file_edit.text()

        self.log_text.append(f"开始传输: 串口 {port}, 波特率 {baud}, 文件 {filepath}")
        self.send_btn.setEnabled(False)

        self.worker = self.build_worker(port, baud, filepath)
        self.worker.log_signal.connect(self.log_text.append)
        self.worker.finished.connect(lambda: self.send_btn.setEnabled(True))
        self.worker.start()


# ============================================================
#  音频页面：位置下拉 + 5 个调试按钮
# ============================================================
class AudioPage(TransferPage):
    def __init__(self):
        super().__init__(
            worker_class=AudioWorker,
            file_dialog_title="选择音频文件",
            file_filter=("Audio Files (*.bin *.pcm *.wav *.mp3);;"
                         "BIN Files (*.bin);;All Files (*)"),
            start_btn_text="开始下载"
        )
        self.debug_workers = []

    def _build_extra_controls(self, layout):
        # 音频位置
        idx_layout = QHBoxLayout()
        idx_layout.addWidget(QLabel("存储位置:"))
        self.audio_index_combo = QComboBox()
        self.audio_index_combo.addItems(["1", "2", "3", "4", "5"])
        idx_layout.addWidget(self.audio_index_combo)
        idx_layout.addStretch()
        layout.addLayout(idx_layout)

        # 调试播放按钮
        debug_layout = QHBoxLayout()
        debug_layout.addWidget(QLabel("调试播放:"))
        self.debug_btns = []
        for i in range(1, 6):
            btn = QPushButton(str(i))
            btn.setFixedWidth(40)
            btn.clicked.connect(lambda _, idx=i: self.send_debug_play(idx))
            debug_layout.addWidget(btn)
            self.debug_btns.append(btn)
        debug_layout.addStretch()
        layout.addLayout(debug_layout)

    def build_worker(self, port, baud, filepath):
        audio_index = int(self.audio_index_combo.currentText())
        audio_name = os.path.basename(filepath)
        return AudioWorker(port, baud, filepath, audio_name, audio_index)

    def send_debug_play(self, audio_index):
        port = self.port_combo.currentText()
        if not port:
            self.log_text.append("⚠️ 未选择串口，无法发送调试命令")
            return
        if self.worker and self.worker.isRunning():
            self.log_text.append("⚠️ 传输进行中，暂不发送调试命令")
            return

        baud = int(self.baud_combo.currentText())
        for btn in self.debug_btns:
            btn.setEnabled(False)

        worker = DebugWorker(port, baud, audio_index)
        worker.log_signal.connect(self.log_text.append)
        worker.finished.connect(self._on_debug_finished)
        self.debug_workers.append(worker)
        worker.start()

    def _on_debug_finished(self):
        for btn in self.debug_btns:
            btn.setEnabled(True)
        self.debug_workers = [w for w in self.debug_workers if w.isRunning()]


# ============================================================
#  主窗口
# ============================================================
class MainWindow(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("STM32 传输工具")
        self.resize(660, 560)

        self.tabs = QTabWidget()

        # 固件升级页
        self.ota_page = TransferPage(
            worker_class=OTAWorker,
            file_dialog_title="选择固件",
            file_filter="BIN Files (*.bin);;All Files (*)",
            start_btn_text="开始升级"
        )
        self.tabs.addTab(self.ota_page, "固件升级")

        # 音频下载页
        self.audio_page = AudioPage()
        self.tabs.addTab(self.audio_page, "音频下载")

        layout = QVBoxLayout()
        layout.addWidget(self.tabs)
        self.setLayout(layout)


if __name__ == "__main__":
    app = QApplication(sys.argv)
    win = MainWindow()
    win.show()
    sys.exit(app.exec())