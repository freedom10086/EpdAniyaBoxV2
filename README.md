# Aniya Box v3.0 Beta

[v1.0地址](https://github.com/freedom10086/EpdAniyaBox)
[v2.0地址](https://github.com/freedom10086/EpdAniyaBoxV2/tree/v2)

### 改进
- 主控换成esp32h2(不支持wifi仅支持蓝牙)功耗更低
- 供电方案由LDO换成DC/DC,主控TPS62740DSSR
- 新增加速度传感器LIS3DH支持震动唤醒，自动检测屏幕方向
- 新增蜂鸣器 支持播放音频
- 新增时钟芯片Max31328（太贵了）支持日历时间和闹钟
- 新增气压传感器SPL06
- 新增光线强度传感器OPT3001
- 温湿度传感器换成SHT40精度更高
- 新增环境气体传感器SGP30


### 缺陷
- OPT3001 和 SHT40 地址一样 后续换成BH1750
- SGP30 功耗巨大初始化时间巨长不适合加入
- LIS3DH 中断引脚为pull/push模式不是open drain 不能和时钟中断接一起上拉
- 缺GPIO 能不能优化程序使用3SPI节省一个DC引脚用于中断
