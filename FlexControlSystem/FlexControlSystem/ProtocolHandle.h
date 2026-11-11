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
    virtual void LightOnProtocol(const uint8_t u8Index, const uint8_t u8Brightness,
                                 const uint16_t u16KeepTime,
                                 QByteArray &qbtSendCmd, QByteArray &qbtRespond)
    { Q_UNUSED(u8Index); Q_UNUSED(u8Brightness); Q_UNUSED(u16KeepTime);
      Q_UNUSED(qbtSendCmd); Q_UNUSED(qbtRespond); }

    // 关闭光源(0x0D/0x0E): u8Index = 1/2 只关指定光源, 0 关闭全部
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
};


#endif // PROTOCOLHANDLE_H
