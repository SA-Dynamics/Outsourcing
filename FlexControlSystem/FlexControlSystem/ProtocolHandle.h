#ifndef PROTOCOLHANDLE_H
#define PROTOCOLHANDLE_H

#include <QObject>

namespace GeneralMotion
{
    struct MotionParams
    {
        enum class MotionIndex
        {
            MotionUp,
            MotionDown,
            MotionLeft,
            MotionRight,
            MotionGather,
            MotionHCentered,
            MotionVCentered,
            MotionDisperse,
            MotionFlip,
            MotionBin,
        };

        MotionIndex eIndex;
        float fVoltage;
        float fFrequency;
        float fTimeUse;
        int16_t s16AngleOffset;
        // 位置修正系数(协议 "2.运动" 数据域 10~11 byte, 有符号):
        // 目前只有水平居中/垂直居中用它, 其他运动固定 0
        int8_t s8ModifyCoeff;
        // 数据域末尾追加的 1 字节(协议 2026-11-22 扩展):
        // 1 = 无限时间动作(一直动作到收到停止指令), 0 = 按 fTimeUse 计时
        // 振动盘 0x03 在数据域第 12 字节, 料仓 0x07 在第 8 字节
        // 给默认值 false: 调用处漏赋值时退化为"按时间动作", 不会发出随机字节
        bool bInfiniteTime = false;
    };
}

class ProtocolHandle : public QObject
{
    Q_OBJECT

public:

//    Q_ENUM(GeneralMotion::MotionParams)

    // 单例模式
    virtual ~ProtocolHandle() = default;


    virtual void MotionControlProtocol(const GeneralMotion::MotionParams &sParams,
                                       QByteArray &qbtSendCmd,
                                       QByteArray &qbtRespond) = 0;

    virtual void SWHeartBeatProtocol(QByteArray &qbtHeart, QByteArray &qbtRespond) = 0;

    virtual void MotionStopProtocol(QByteArray &qbtCmd, QByteArray &qbtRespond) = 0;

    // 料仓运动(0x07): 数据域 7 字节, 下位机运动完成后才回 0x08(与振动盘语义一致)
    virtual void BinMotionProtocol(const GeneralMotion::MotionParams &sParams,
                                   QByteArray &qbtSendCmd,
                                   QByteArray &qbtRespond)
    { Q_UNUSED(sParams); Q_UNUSED(qbtSendCmd); Q_UNUSED(qbtRespond); }

    // 料仓停止(0x09/0x0A)
    virtual void BinMotionStopProtocol(QByteArray &qbtCmd, QByteArray &qbtRespond)
    { Q_UNUSED(qbtCmd); Q_UNUSED(qbtRespond); }

    // 打开光源(0x0B): u16KeepTime 为保持时间, 单位 100ms(即秒数x10)
    // bInfiniteKeep = true 时数据域末尾追加 1(无限保持, 直到关闭指令);
    // 下位机把它等价于"保持时间 0 = 常亮", 但仍按协议显式下发该字节
    virtual void LightOnProtocol(const uint8_t u8Index, const uint8_t u8Brightness,
                                 const uint16_t u16KeepTime, const bool bInfiniteKeep,
                                 QByteArray &qbtSendCmd, QByteArray &qbtRespond)
    { Q_UNUSED(u8Index); Q_UNUSED(u8Brightness); Q_UNUSED(u16KeepTime); Q_UNUSED(bInfiniteKeep);
      Q_UNUSED(qbtSendCmd); Q_UNUSED(qbtRespond); }

    // 关闭光源(0x0D/0x0E): u8Index = 1/2/3/4 只关 A/B/C/D, 0 关闭全部
    // 应答 0x0E 无数据域, 关哪一路都是同一帧
    virtual void LightOffProtocol(const uint8_t u8Index, QByteArray &qbtCmd, QByteArray &qbtRespond)
    { Q_UNUSED(u8Index); Q_UNUSED(qbtCmd); Q_UNUSED(qbtRespond); }

    // 保存IP(0x0F/0x10): 下发 4 字节 IP, 下位机写入 Flash 后重启
    // 应答有两种(1=成功 / 0=失败), 无法像其他指令那样预先构造期望帧,
    // 因此这里只生成下发帧, 应答由 ConnectiveHandle 按命令码 0x10 匹配
    virtual void SaveIpProtocol(const QByteArray &qbtIp, QByteArray &qbtCmd)
    { Q_UNUSED(qbtIp); Q_UNUSED(qbtCmd); }

    // 获取IP(0x11/0x12): 下发 1 字节 0x01, 应答 4 字节 IP(数据域为变长值,
    // 无法预构造期望帧), 应答由 ConnectiveHandle 按命令码 0x12 匹配
    virtual void GetIpProtocol(QByteArray &qbtCmd)
    { Q_UNUSED(qbtCmd); }

    // 获取电压(0x13/0x14): 下发 1 字节 0x01, 应答 2 字节 电压x10(大端),
    // 应答由 ConnectiveHandle 按命令码 0x14 匹配
    virtual void GetVoltageProtocol(QByteArray &qbtCmd)
    { Q_UNUSED(qbtCmd); }
};


#endif // PROTOCOLHANDLE_H
