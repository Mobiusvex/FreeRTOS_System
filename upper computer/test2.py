import random

# 设置固定种子，确保每次运行生成的数组都一样（保证两端匹配）
random.seed(2026992108)  

# 生成 240 个 0~255 的随机字节
key_bytes = [random.randint(0, 255) for _ in range(240)]

# 打印为 C 语言数组格式（每行16个，方便阅读）
print("const uint8_t KEY_STREAM[240] = {")
for i in range(0, 240, 16):
    # 取出 16 个，格式化为 0xXX 并用逗号连接
    line = ", ".join(f"0x{byte:02X}" for byte in key_bytes[i:i+16])
    print(f"    {line},")
print("};")