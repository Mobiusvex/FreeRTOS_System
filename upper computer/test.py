import sys
import serial
import serial.tools.list_ports
from PyQt6.QtWidgets import (QApplication, QWidget, QPushButton, QVBoxLayout, 
                             QHBoxLayout, QComboBox, QLineEdit, QTextEdit, 
                             QFileDialog, QLabel)
from PyQt6.QtCore import QThread, pyqtSignal
import struct      # 用来把数字打包成字节
import binascii    # 用来计算CRC32
import struct
import binascii
import time

# 上位机顶部
TIMEOUT_DATA_PKT  = 0.5     # 数据包/起始包：500ms 足够
TIMEOUT_END_PKT   = 30.0    # 结束包：STM32 要读全片算CRC，给30秒

KEY_STREAM =  bytes([
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

class OTAWorker(QThread):
    log_signal = pyqtSignal(str)

    def __init__(self, port, baudrate, filepath):
        super().__init__()
        self.port = port
        self.baudrate = baudrate
        self.filepath = filepath
 # ★ 辅助：格式化耗时
    @staticmethod
    def _elapsed_str(t_start: float) -> str:
        """把 time.time() 差值格式化成 '1分23.4秒' 这种可读字符串"""
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
    # 辅助：计算CRC16（取CRC32低16位，小端打包）
    @staticmethod
    def calc_crc16(data: bytes) -> bytes:
        crc32 = binascii.crc32(data) & 0xffffffff
        return struct.pack('<H', crc32 & 0xffff)

    # 辅助：发送一帧并等待ACK（返回True表示成功）
    def send_frame_and_wait_ack(self, ser: serial.Serial, frame: bytes, expected_seq: int,timeout_s=2) -> bool:
        # 清空输入缓冲区，去除残余数据
        ser.reset_input_buffer()
        # 发送帧
        ser.write(frame)
        ser.flush()
         # 循环读满 9 字节
        reply = b''
        t_end = time.time() + timeout_s
        while len(reply) < 9 and time.time() < t_end:
            chunk = ser.read(9 - len(reply))
            if chunk:
                reply += chunk
        #显示接收包---------------------------------------------------------------------
        hex_frame = ' '.join(f'{b:02X}' for b in reply)  # 只显示前16字节
        self.log_signal.emit(f"(接收数据内容: {hex_frame}...)")
        #显示包----------------------------------------------------------------------
        # return True
        if len(reply) != 9:
            self.log_signal.emit(f"⚠️ 回复长度错误，期望9字节，收到{len(reply)}")
            return False
        # 检查固定字段
        if reply[0] != 0x68 or reply[-1] != 0x86 or reply[3] != 0x13:
            self.log_signal.emit(f"⚠️ 回复格式错误: {reply.hex()}")
            return False
        # 检查序号是否匹配
        recv_seq = struct.unpack('<H', reply[1:3])[0]
        if recv_seq != expected_seq:
            self.log_signal.emit(f"⚠️ 序号不匹配，期望{expected_seq}，收到{recv_seq}")
            return False
        # 检查数据域 (数据长度0x01，数据0x66成功 / 0x5X失败)
        if reply[5] == 0x66:
            return True
        else:
            self.log_signal.emit(f"❌ STM32回复失败(0x{reply[5]:02X})")
            return False

    def run(self):
        t_start = time.time()
        ser = None
        try:
            # 1. 打开串口
            t_open = time.time()  # ★ 打开串口计时
            ser = serial.Serial(self.port, self.baudrate, timeout=3)
            self.log_signal.emit(f"✅ 打开串口 {self.port} 成功 "
                                f"(耗时 {self._elapsed_str(t_open)})")

            # 2. 读取原始固件
            with open(self.filepath, 'rb') as f:
                firmware_raw = f.read()
            total_size = len(firmware_raw)
            if total_size == 0:
                self.log_signal.emit("❌ 固件文件为空！")
                return

            # 3. 计算总CRC（基于原始明文，用于结束包校验）
            total_crc32 = binascii.crc32(firmware_raw) & 0xffffffff
            self.log_signal.emit(f"📦 固件大小: {total_size} 字节")
            self.log_signal.emit(f"🔑 总CRC32: {total_crc32:08X}")

            # 4. 分包参数（每包最多240字节）
            # MAX_DATA = 240
            # total_packets = (total_size + MAX_DATA - 1) // MAX_DATA
            MAX_DATA = 240
            original_size=total_size
            pad_len = (MAX_DATA - (len(firmware_raw) % MAX_DATA)) % MAX_DATA
            if pad_len > 0:
                firmware_raw += b'\xFF' * pad_len
                self.log_signal.emit(f"📏 补齐 {pad_len} 字节 0xFF 到 240 整数倍")

            total_size = len(firmware_raw)   # 此时已经是 240 的整数倍
            total_crc32 = binascii.crc32(firmware_raw) & 0xffffffff
            total_packets = total_size // MAX_DATA   # 精确整除
            self.log_signal.emit(f"📨 将分 {total_packets} 包发送 (每包{MAX_DATA}字节)")

            # ========== 发送起始包 ==========
            # 起始包：起始(0x68) + 序号(0x0000) + 指令(0x11) + 数据长度(0x02) + 数据(总包数2B小端)
            start_header = struct.pack('<B', 0x68) + struct.pack('<H', 0) + struct.pack('<B', 0x11) + struct.pack('<B', 6)
            start_data = struct.pack('<H', total_packets)+ struct.pack('<I', original_size)   # 总包数，小端
            start_frame_without_crc = start_header + start_data
            start_crc = self.calc_crc16(start_frame_without_crc)
            start_frame = start_frame_without_crc + start_crc + struct.pack('<B', 0x86)

            self.log_signal.emit("📤 发送起始包...")

            #显示起始包---------------------------------------------------------------------
            hex_start_frame = ' '.join(f'{b:02X}' for b in start_frame)  # 只显示前16字节
            self.log_signal.emit(f"(发送数据内容: {hex_start_frame}...)")
            #显示结束包----------------------------------------------------------------------

            # 起始包序号为0，等待STM32回复序号0
            t_start_pkt = time.time()   # ★ 起始包计时
            ok = self.send_frame_and_wait_ack(ser, start_frame, expected_seq=0,timeout_s=TIMEOUT_DATA_PKT)
            if not ok:
                self.log_signal.emit(f"❌ 起始包确认失败 (耗时 {self._elapsed_str(t_start_pkt)})，升级终止")
                self.log_signal.emit(f"⏱️ 总耗时: {self._elapsed_str(t_start)}")
                return
            self.log_signal.emit(f"✅ 起始包确认成功 (耗时 {self._elapsed_str(t_start_pkt)})")

            # ========== 循环发送数据包 ==========
            t_data_start = time.time()   # ★ 数据包总计时
            for packet_idx in range(total_packets):
                # 取原始数据块
                offset = packet_idx * MAX_DATA
                chunk_raw = firmware_raw[offset : offset + MAX_DATA]
                chunk_len = len(chunk_raw)

                # ---- 加密：逐字节异或240字节密钥流 ----
                # 绝对偏移 = packet_idx * MAX_DATA，保证不同包使用不同密钥片段
                abs_offset = offset
                encrypted_chunk = bytes([
                    b ^ KEY_STREAM[(abs_offset + j) % len(KEY_STREAM)]
                    for j, b in enumerate(chunk_raw)
                ])

                # ---- 组包 ----
                # 头部：起始 + 序号 + 指令0x12 + 数据长度(1B)
                header = struct.pack('<B', 0x68) + struct.pack('<H', packet_idx) + struct.pack('<B', 0x12) + struct.pack('<B', chunk_len)
                frame_without_crc = header + encrypted_chunk
                crc = self.calc_crc16(frame_without_crc)
                data_frame = frame_without_crc + crc + struct.pack('<B', 0x86)
                # 显示数据包--------------------------------------------------------------------
                hex_preview = ' '.join(f'{b:02X}' for b in data_frame[:16])  # 只显示前16字节
                hex_endview = ' '.join(f'{b:02X}' for b in data_frame[-16:])  # 只显示前16字节
                self.log_signal.emit(f"(发送数据前16字节: {hex_preview}...{hex_endview})")
                # -------------------------------------------------------------------------------------
                # ---- 发送与重试 ----
                success = False
                for retry in range(3):
                    self.log_signal.emit(f"📤 发送第 {packet_idx+1}/{total_packets} 包 (重试{retry})")
                    ok = self.send_frame_and_wait_ack(ser, data_frame, expected_seq=packet_idx,timeout_s=TIMEOUT_DATA_PKT)
                    if ok:
                        self.log_signal.emit(f"✅ 第 {packet_idx+1} 包成功")
                        success = True
                        break
                    else:
                        self.log_signal.emit(f"⚠️ 第 {packet_idx+1} 包失败，重试 {retry+1}/3")
                        # 重试前等待一小段时间
                        time.sleep(0.1)
                if not success:
                    self.log_signal.emit(f"❌ 第 {packet_idx+1} 包发送失败，升级终止")
                    self.log_signal.emit(f"⏱️ 已发送 {packet_idx}/{total_packets} 包，"
                                         f"数据阶段耗时 {self._elapsed_str(t_data_start)}")
                    self.log_signal.emit(f"⏱️ 总耗时: {self._elapsed_str(t_start)}")
                    return
                # ★ 每 100 包打印一次进度 + 实时速度
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
            # ========== 发送结束包 ==========
            # 结束包：起始(0x68) + 序号=总包数 + 指令0x14 + 数据长度0x04 + 数据(总CRC32 4B小端)
            end_header = struct.pack('<B', 0x68) + struct.pack('<H', total_packets) + struct.pack('<B', 0x14) + struct.pack('<B', 4)
            end_data = struct.pack('<I', total_crc32)   # 小端4字节
            end_frame_without_crc = end_header + end_data
            end_crc = self.calc_crc16(end_frame_without_crc)
            end_frame = end_frame_without_crc + end_crc + struct.pack('<B', 0x86)
        
            self.log_signal.emit("📤 发送结束包...")

            #结尾包---------------------------------------------------------------------
            hex_end_frame = ' '.join(f'{b:02X}' for b in end_frame)  # 只显示前16字节
            self.log_signal.emit(f"(发送数据内容 {hex_end_frame})")
            #-----------------------------------------------------------------------------
            # 发送结束包
            t_end_pkt = time.time()   # ★ 结束包计时
            ok = self.send_frame_and_wait_ack(ser, end_frame, expected_seq=total_packets,timeout_s=TIMEOUT_END_PKT)
            
            total_elapsed = time.time() - t_start
            if ok:
                self.log_signal.emit(f"🎉 OTA 升级全部完成！总校验通过！")
                self.log_signal.emit(f"⏱️ 结束包校验耗时: {self._elapsed_str(t_end_pkt)}")
                self.log_signal.emit(f"⏱️ 数据阶段耗时: {self._elapsed_str(t_data_start)}")
                self.log_signal.emit(f"⭐ 总耗时: {self._elapsed_str(t_start)}")
                # ★ 计算平均速度（KB/s）
                if total_elapsed > 0:
                    speed_kbps = original_size / 1024 / total_elapsed
                    self.log_signal.emit(f"⚡ 平均速率: {speed_kbps:.2f} KB/s")
            else:
                self.log_signal.emit(f"❌ 结束包确认失败，升级可能不完整！")
                self.log_signal.emit(f"⏱️ 总耗时: {self._elapsed_str(t_start)}")

        except Exception as e:
            self.log_signal.emit(f"💥 发生异常: {e}")
        finally:
            if ser and ser.is_open:
                ser.close()
                self.log_signal.emit("🔌 串口已关闭")

class OTAWindow(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("STM32 OTA 升级工具")
        self.resize(600, 400)

        # 布局
        main_layout = QVBoxLayout()

        # 串口选择行
        port_layout = QHBoxLayout()
        port_layout.addWidget(QLabel("串口:"))
        self.port_combo = QComboBox()
        self.refresh_ports()
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
        self.file_edit.setPlaceholderText("请选择固件文件 (.bin)")
        self.file_btn = QPushButton("浏览")
        self.file_btn.clicked.connect(self.select_file)
        file_layout.addWidget(self.file_edit)
        file_layout.addWidget(self.file_btn)
        main_layout.addLayout(file_layout)

        # 发送按钮
        self.send_btn = QPushButton("开始升级")
        self.send_btn.clicked.connect(self.start_ota)
        main_layout.addWidget(self.send_btn)

        # 日志显示
        self.log_text = QTextEdit()
        self.log_text.setReadOnly(True)
        main_layout.addWidget(self.log_text)

        self.setLayout(main_layout)

        # OTA 工作线程
        self.worker = None

    def refresh_ports(self):
        """刷新可用串口列表"""
        self.port_combo.clear()
        ports = serial.tools.list_ports.comports()
        for port in ports:
            self.port_combo.addItem(port.device)

    def select_file(self):
        """选择固件文件"""
        file_path, _ = QFileDialog.getOpenFileName(self, "选择固件", "", "BIN Files (*.bin);;All Files (*)")
        if file_path:
            self.file_edit.setText(file_path)

    def start_ota(self):
        """开始OTA升级"""
        if not self.file_edit.text():
            self.log_text.append("请先选择固件文件")
            return
        if self.worker and self.worker.isRunning():
            self.log_text.append("升级正在进行中...")
            return

        port = self.port_combo.currentText()
        baud = int(self.baud_combo.currentText())
        filepath = self.file_edit.text()

        self.log_text.append(f"开始升级: 串口 {port}, 波特率 {baud}, 文件 {filepath}")
        self.worker = OTAWorker(port, baud, filepath)
        self.worker.log_signal.connect(self.log_text.append)
        self.worker.start()

if __name__ == "__main__":
    app = QApplication(sys.argv)
    win = OTAWindow()
    win.show()
    sys.exit(app.exec())