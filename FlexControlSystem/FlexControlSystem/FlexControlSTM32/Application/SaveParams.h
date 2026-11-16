/**
 * @file    SaveParams.h
 * @brief   掉电保存参数(Flash)读写: 目前只保存网络 IP
 * @note    与通信内核配套: 上位机下发"保存IP"后写 Flash, 下次上电 NetworkInit
 *          优先用 Flash 里的值, 这样设备换网段后不用重新烧固件。
 *          协议命令本身由应用层解释, 本模块只负责存取。
 *          上位机的保存命令只带 4 字节 IP, 掩码/网关由 IP 推导后一起落盘,
 *          以后要扩展成"IP+掩码+网关"三字段时, Flash 里的数据结构不用改。
 */
#ifndef SAVEPARAMS_H
#define SAVEPARAMS_H

#include "main.h"
#include <stdbool.h>

/* ============================ Flash 布局 ==================================== */

/*
 * STM32F103C8T6: 64KB Flash, 页大小 1KB(中容量产品), 共 64 页。
 * 取最后一页存放参数, 当前固件约 29KB, 与之相距很远, 不会被代码覆盖。
 *
 * 换型号时必须改这里:
 *   F103CB(128KB) -> 0x0801FC00
 *   F103RC(256KB) -> 0x0803FC00
 */
#define SAVE_PARAMS_FLASH_ADDR          0x0800FC00u
#define SAVE_PARAMS_FLASH_PAGE_SIZE     1024u

/* 结构体标识与版本: 换布局时把版本 +1, 旧数据会自动判为无效并回退默认值 */
#define SAVE_PARAMS_MAGIC               0x53564E50u     /* "SVNP" */
#define SAVE_PARAMS_VERSION             0x0001u

/* ============================ 数据结构 ====================================== */

/**
 * @brief Flash 中保存的参数块
 * @note  写入前整个结构体先清零, 因此填充字节是确定的 0, 可以按 sizeof
 *        整体写入/回读比较, 不会因结构体对齐填充导致校验失败。
 */
typedef struct
{
    uint32_t u32Magic;          /**< 标识, 判断这一页是否写过            */
    uint16_t u16Version;        /**< 布局版本                            */
    uint8_t  u8Ip[4];           /**< 本机 IP                             */
    uint8_t  u8Subnet[4];       /**< 子网掩码                            */
    uint8_t  u8Gateway[4];      /**< 网关                                */
    uint8_t  u8Reserved[6];     /**< 预留(后续可放波特率/端口等)         */
    uint16_t u16CheckSum;       /**< 前面所有字节的累加和(简单校验)      */
} SaveParams_t;

/* ============================ 对外接口 ====================================== */

/**
 * @brief  上电读取 Flash 参数, 校验失败时标记为"未保存过"
 * @note   只读不写、不阻塞; 必须在 NetworkInit 之前调用
 */
void SaveParamsInit(void);

/**
 * @brief  Flash 中是否存在有效参数
 * @return true=有(已通过标识/版本/校验三项检查), false=没有
 */
bool SaveParamsIsStored(void);

/**
 * @brief  读取保存的 IP
 * @param  pIp 输出缓冲, 4 字节
 * @return true=读出成功, false=没有有效参数(调用方回退默认值)
 */
bool SaveParamsGetIp(uint8_t *pIp);

/**
 * @brief  读取保存的子网掩码
 * @param  pSubnet 输出缓冲, 4 字节
 * @return true=读出成功, false=没有有效参数
 */
bool SaveParamsGetSubnet(uint8_t *pSubnet);

/**
 * @brief  读取保存的网关
 * @param  pGateway 输出缓冲, 4 字节
 * @return true=读出成功, false=没有有效参数
 */
bool SaveParamsGetGateway(uint8_t *pGateway);

/**
 * @brief  保存 IP 到 Flash(掩码固定 255.255.255.0, 网关取同网段 .1)
 * @param  pIp 4 字节 IP
 * @return true=写入并回读校验成功, false=擦除/编程/校验失败
 * @note   擦除一页约 20~40ms, 期间 CPU 从 Flash 取指会停顿, 调用前应先停
 *         执行器。IP 与已保存值相同时直接返回 true, 不做无谓擦写。
 */
bool SaveParamsSaveIp(const uint8_t *pIp);

#endif
