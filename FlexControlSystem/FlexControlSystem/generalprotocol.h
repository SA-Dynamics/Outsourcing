#ifndef GENERALPROTOCOL_H
#define GENERALPROTOCOL_H

#include "ProtocolHandle.h"


namespace GeneralProtocolItems
{
    constexpr uint8_t u8FRAME_HEAD1 = 0x55;
    constexpr uint8_t u8FRAME_HEAD2 = 0xAA;
    constexpr uint8_t u8FRAME_TAIL1 = 0xAA;
    constexpr uint8_t u8FRAME_TAIL2 = 0x55;

    constexpr uint8_t u8SW_HEART_BEAT_CMD = 0x01;
    constexpr uint8_t u8FW_HEART_BEAT_RESPOND = 0x02;

    constexpr uint8_t u8SW_MOTION_CMD = 0x03;
    constexpr uint8_t u8FW_MOTION_RESPOND = 0x04;

    constexpr uint8_t u8SW_MOTION_STOP_CMD = 0x05;
    constexpr uint8_t u8FW_MOTION_STOP_RESPOND = 0x06;

    constexpr uint8_t u8SW_BIN_MOTION_CMD = 0x07;
    constexpr uint8_t u8FW_BIN_MOTION_RESPOND = 0x08;

    constexpr uint8_t u8SW_BIN_MOTION_STOP_CMD = 0x09;
    constexpr uint8_t u8FW_BIN_MOTION_STOP_RESPOND = 0x0A;

    constexpr uint8_t u8SW_LIGHT_ON_CMD = 0x0B;
    constexpr uint8_t u8FW_LIGHT_ON_RESPOND = 0x0C;

    constexpr uint8_t u8SW_LIGHT_OFF_CMD = 0x0D;
    constexpr uint8_t u8FW_LIGHT_OFF_RESPOND = 0x0E;

    // 保存IP: 下发 4 字节 IP, 应答 1 字节(1=保存成功, 0=保存失败)
    constexpr uint8_t u8SW_SAVE_IP_CMD = 0x0F;
    constexpr uint8_t u8FW_SAVE_IP_RESPOND = 0x10;
    constexpr uint8_t u8SAVE_IP_SUCCESS = 0x01;

    // 获取IP: 下发 1 字节 0x01, 应答 4 字节 IP
    constexpr uint8_t u8SW_GET_IP_CMD = 0x11;
    constexpr uint8_t u8FW_GET_IP_RESPOND = 0x12;

    // 光源索引
    constexpr uint8_t u8SW_LIGHT_INDEX_A = 0x01;
    constexpr uint8_t u8SW_LIGHT_INDEX_B = 0x02;
    constexpr uint8_t u8SW_LIGHT_INDEX_ALL = 0x00;      // 仅关闭光源使用: 关闭全部

    constexpr uint8_t u8SW_MOTION_UP = 0x01;
    constexpr uint8_t u8SW_MOTION_DOWN = 0x02;
    constexpr uint8_t u8SW_MOTION_LEFT = 0x03;
    constexpr uint8_t u8SW_MOTION_RIGHT = 0x04;
    constexpr uint8_t u8SW_MOTION_GATHER = 0x05;
    constexpr uint8_t u8SW_MOTION_HCENTER = 0x06;
    constexpr uint8_t u8SW_MOTION_VCENTER = 0x07;
    constexpr uint8_t u8SW_MOTION_DISPERSE = 0x08;
}


class GeneralProtocol : public ProtocolHandle
{
public:
    // 单例模式
    static GeneralProtocol& getInstance()
    {
        static GeneralProtocol instance;
        return instance;
    }

    GeneralProtocol(const GeneralProtocol&) = delete;
    GeneralProtocol& operator=(const GeneralProtocol&) = delete;

    void MotionControlProtocol(const GeneralMotion::MotionParams &sParams,
                                       QByteArray &qbtSendCmd,
                                       QByteArray &qbtRespond) override;

    void SWHeartBeatProtocol(QByteArray &qbtHeart, QByteArray &qbtRespond) override;

    void MotionStopProtocol(QByteArray &qbtCmd, QByteArray &qbtRespond) override;

    void BinMotionProtocol(const GeneralMotion::MotionParams &sParams,
                           QByteArray &qbtSendCmd,
                           QByteArray &qbtRespond) override;

    void BinMotionStopProtocol(QByteArray &qbtCmd, QByteArray &qbtRespond) override;

    void LightOnProtocol(const uint8_t u8Index, const uint8_t u8Brightness,
                         const uint16_t u16KeepTime,
                         QByteArray &qbtSendCmd, QByteArray &qbtRespond) override;

    void LightOffProtocol(const uint8_t u8Index, QByteArray &qbtCmd, QByteArray &qbtRespond) override;

    void SaveIpProtocol(const QByteArray &qbtIp, QByteArray &qbtCmd) override;

    void GetIpProtocol(QByteArray &qbtCmd) override;

private:
    GeneralProtocol() = default;
    ~GeneralProtocol() override;

    void CalcCheckSum(QByteArray &qbtSendCmd);

    uint8_t GetMotionIndexCmd(const GeneralMotion::MotionParams::MotionIndex eIndex);
    void AddCheckSum(QByteArray &qbtData);

};

#endif // GENERALPROTOCOL_H
