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

    // 获取电压: 下发 1 字节 0x01, 应答 2 字节 电压x10(大端)
    constexpr uint8_t u8SW_GET_VOLTAGE_CMD = 0x13;
    constexpr uint8_t u8FW_GET_VOLTAGE_RESPOND = 0x14;

    // 获取固件参数: 下发 1 字节 0x01, 应答 49 字节(1 标志 + 12 个 float)
    constexpr uint8_t u8SW_GET_KEY_PARAMS_CMD = 0x15;
    constexpr uint8_t u8FW_GET_KEY_PARAMS_RESPOND = 0x16;
    // 应答首字节: 0=Flash 里有有效参数, 非 0=Flash 无效, 回的是默认值
    constexpr uint8_t u8GET_KEY_PARAMS_FROM_FLASH = 0x00;

    // 保存固件参数: 下发 49 字节(首字节固定 1 + 12 个 float), 应答 1 字节
    constexpr uint8_t u8SW_SAVE_KEY_PARAMS_CMD = 0x17;
    constexpr uint8_t u8FW_SAVE_KEY_PARAMS_RESPOND = 0x18;
    // 应答字节: 0=保存成功, 1=保存失败(注意与保存IP 的 1=成功 相反)
    constexpr uint8_t u8SAVE_KEY_PARAMS_SUCCESS = 0x00;
    // 下发帧数据域首字节固定 1(协议 0x17 定义), 下位机靠它识别这是"保存"请求
    constexpr uint8_t u8SAVE_KEY_PARAMS_FLAG = 0x01;
    // 12 个 float 的裸字节数 = 48, 下发帧数据域 = 1 + 48 = 49
    constexpr int iKEY_PARAMS_DATA_LEN = 48;

    // 获取固件版本(2026-11-25 二十二次, 协议第 10 节新增):
    // 下发 1 字节 0x01, 应答 3 字节 = major / minor / patch。
    // 上位机只在连接成功后读一次(固件是死的, 版本不会变), 不做周期轮询。
    constexpr uint8_t u8SW_GET_FW_VERSION_CMD = 0x19;
    constexpr uint8_t u8FW_GET_FW_VERSION_RESPOND = 0x1A;
    // 应答数据域长度: 主/次/修订 各 1 字节
    constexpr int iFW_VERSION_DATA_LEN = 3;

    // 光源索引
    constexpr uint8_t u8SW_LIGHT_INDEX_A = 0x01;
    constexpr uint8_t u8SW_LIGHT_INDEX_B = 0x02;
    constexpr uint8_t u8SW_LIGHT_INDEX_C = 0x03;
    constexpr uint8_t u8SW_LIGHT_INDEX_D = 0x04;
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
                         const uint32_t u32KeepTime, const bool bInfiniteKeep,
                         QByteArray &qbtSendCmd, QByteArray &qbtRespond) override;

    void LightOffProtocol(const uint8_t u8Index, QByteArray &qbtCmd, QByteArray &qbtRespond) override;

    // 光源关闭完成期望帧(0x0E + 1 字节索引): 保持时间到时下位机自发该帧,
    // 引擎用它生成期望帧做队列匹配
    void LightOffFinishExpect(const uint8_t u8Index, QByteArray &qbtRespond);

    void SaveIpProtocol(const QByteArray &qbtIp, QByteArray &qbtCmd) override;

    void GetIpProtocol(QByteArray &qbtCmd) override;

    // 获取电压: 应答由引擎按命令码 0x14 单独匹配, 不走整帧比对
    void GetVoltageProtocol(QByteArray &qbtCmd) override;

    // 获取/保存固件参数: 应答内容(12 个 float 或 1 字节结果)无法预构造期望帧,
    // 同样由引擎按命令码 0x16 / 0x18 单独匹配
    void GetKeyParamsProtocol(QByteArray &qbtCmd) override;
    void SaveKeyParamsProtocol(const GeneralKeyParams::KeyParams &sParams, QByteArray &qbtCmd) override;

    // 获取固件版本: 应答 3 字节随固件变化, 无法预构造期望帧,
    // 由引擎按命令码 0x1A 单独匹配(与获取电压/获取固件参数同构)
    void GetFirmwareVersionProtocol(QByteArray &qbtCmd) override;

    // 12 个 float <-> 48 字节大端裸数据(协议 0x16/0x17 的数据域主体)。
    // 单独抽出来: 引擎解析应答和界面填 spinbox 都复用这一对函数,
    // 避免两处各写一遍移位, 日后改字节序只改一处
    static void KeyParamsToBytes(const GeneralKeyParams::KeyParams &sParams, QByteArray &qbtData);
    static bool KeyParamsFromBytes(const QByteArray &qbtData, GeneralKeyParams::KeyParams &sParams);

private:
    GeneralProtocol() = default;
    ~GeneralProtocol() override;

    void CalcCheckSum(QByteArray &qbtSendCmd);

    uint8_t GetMotionIndexCmd(const GeneralMotion::MotionParams::MotionIndex eIndex);
    void AddCheckSum(QByteArray &qbtData);

};

#endif // GENERALPROTOCOL_H
