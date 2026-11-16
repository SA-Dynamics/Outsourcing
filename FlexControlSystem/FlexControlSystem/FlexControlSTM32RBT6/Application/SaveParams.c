/**
 * @file    SaveParams.c
 * @brief   掉电保存参数(Flash)读写实现
 * @note    F103 中容量 Flash 的最小写入单位是半字(2 字节), 擦除以页(1KB)为
 *          单位。流程固定为: 解锁 -> 清标志 -> 擦页 -> 逐半字编程 -> 上锁 ->
 *          回读比较。任何一步失败都立即上锁返回, 不留隐患。
 */
#include "SaveParams.h"
#include <stddef.h>
#include <string.h>

/* 校验和覆盖范围: 结构体起始到 u16CheckSum 之前的所有字节 */
#define SAVE_PARAMS_CHECK_LEN       ((uint16_t)offsetof(SaveParams_t, u16CheckSum))

/* 半字数量: 结构体已按半字对齐, sizeof 为偶数 */
#define SAVE_PARAMS_HALFWORD_NUM    ((uint16_t)(sizeof(SaveParams_t) / 2u))

/** Flash 参数的 RAM 副本, 上电时由 SaveParamsInit 载入 */
static SaveParams_t g_sParams;

/** 副本是否有效(标识/版本/校验三项都通过) */
static bool g_bValid = false;

/* ============================ 内部函数 ====================================== */

/**
 * @brief  计算校验和: 从结构体起始累加到 u16CheckSum 之前, 取低 16 位
 * @param  pParams 待计算的结构体
 * @return 16 位累加和
 * @note   故意不做反码加一, 越简单越好: 这里防的是"擦除不干净/写入中断"
 *         这类粗错, 不是防篡改。
 */
static uint16_t SaveParamsCalcSum(const SaveParams_t *pParams)
{
    const uint8_t *pByte = (const uint8_t *)pParams;
    uint16_t u16Sum = 0;
    uint16_t i;

    for (i = 0; i < SAVE_PARAMS_CHECK_LEN; i++)
    {
        u16Sum = (uint16_t)(u16Sum + pByte[i]);
    }

    return u16Sum;
}

/**
 * @brief 从 Flash 读一个参数块(直接按地址整体拷贝, 无需解锁)
 */
static void SaveParamsReadFlash(SaveParams_t *pParams)
{
    memcpy(pParams, (const void *)SAVE_PARAMS_FLASH_ADDR, sizeof(SaveParams_t));
}

/* ============================ 对外接口 ====================================== */

void SaveParamsInit(void)
{
    SaveParams_t sTemp;

    SaveParamsReadFlash(&sTemp);

    /* 三项检查: 标识对(不是擦除态) + 版本对(布局没换) + 校验和对(内容完整) */
    if ((sTemp.u32Magic != SAVE_PARAMS_MAGIC) ||
        (sTemp.u16Version != SAVE_PARAMS_VERSION) ||
        (sTemp.u16CheckSum != SaveParamsCalcSum(&sTemp)))
    {
        memset(&g_sParams, 0, sizeof(g_sParams));
        g_bValid = false;
        return;
    }

    memcpy(&g_sParams, &sTemp, sizeof(g_sParams));
    g_bValid = true;
}

bool SaveParamsIsStored(void)
{
    return g_bValid;
}

bool SaveParamsGetIp(uint8_t *pIp)
{
    if ((g_bValid == false) || (pIp == NULL))
    {
        return false;
    }

    memcpy(pIp, g_sParams.u8Ip, sizeof(g_sParams.u8Ip));

    return true;
}

bool SaveParamsGetSubnet(uint8_t *pSubnet)
{
    if ((g_bValid == false) || (pSubnet == NULL))
    {
        return false;
    }

    memcpy(pSubnet, g_sParams.u8Subnet, sizeof(g_sParams.u8Subnet));

    return true;
}

bool SaveParamsGetGateway(uint8_t *pGateway)
{
    if ((g_bValid == false) || (pGateway == NULL))
    {
        return false;
    }

    memcpy(pGateway, g_sParams.u8Gateway, sizeof(g_sParams.u8Gateway));

    return true;
}

bool SaveParamsSaveIp(const uint8_t *pIp)
{
    SaveParams_t sNew;
    SaveParams_t sCheck;
    FLASH_EraseInitTypeDef sErase = {0};
    uint32_t u32PageError = 0;
    const uint16_t *pSrc;
    uint32_t u32Addr;
    uint16_t i;
    bool bResult = false;

    if (pIp == NULL)
    {
        return false;
    }

    /* 已存过且 IP 没变: 不擦不写, 直接算成功(省一次 Flash 磨损) */
    if ((g_bValid != false) && (memcmp(g_sParams.u8Ip, pIp, sizeof(g_sParams.u8Ip)) == 0))
    {
        return true;
    }

    /*
     * 组包: 协议只下发 IP, 掩码/网关按同网段规则推导。
     * 与现场默认配置(192.168.2.199 / 255.255.255.0 / 192.168.2.1)一致,
     * 若将来网关不是 .1, 把这里改成读上位机下发的完整三元组即可。
     */
    memset(&sNew, 0, sizeof(sNew));
    sNew.u32Magic = SAVE_PARAMS_MAGIC;
    sNew.u16Version = SAVE_PARAMS_VERSION;
    memcpy(sNew.u8Ip, pIp, sizeof(sNew.u8Ip));
    sNew.u8Subnet[0] = 255u;
    sNew.u8Subnet[1] = 255u;
    sNew.u8Subnet[2] = 255u;
    sNew.u8Subnet[3] = 0u;
    sNew.u8Gateway[0] = pIp[0];
    sNew.u8Gateway[1] = pIp[1];
    sNew.u8Gateway[2] = pIp[2];
    sNew.u8Gateway[3] = 1u;
    sNew.u16CheckSum = SaveParamsCalcSum(&sNew);

    HAL_FLASH_Unlock();

    /* 先清掉可能残留的错误标志, 否则擦除会直接返回 HAL_ERROR */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);

    sErase.TypeErase   = FLASH_TYPEERASE_PAGES;
    sErase.PageAddress = SAVE_PARAMS_FLASH_ADDR;
    sErase.NbPages     = 1u;

    if (HAL_FLASHEx_Erase(&sErase, &u32PageError) == HAL_OK)
    {
        pSrc = (const uint16_t *)&sNew;
        u32Addr = SAVE_PARAMS_FLASH_ADDR;

        for (i = 0; i < SAVE_PARAMS_HALFWORD_NUM; i++)
        {
            if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, u32Addr, pSrc[i]) != HAL_OK)
            {
                break;
            }
            u32Addr += 2u;
        }

        /* 编程循环完整跑完才做回读校验: 少写一个半字也算失败 */
        if (i >= SAVE_PARAMS_HALFWORD_NUM)
        {
            SaveParamsReadFlash(&sCheck);
            if (memcmp(&sCheck, &sNew, sizeof(sNew)) == 0)
            {
                memcpy(&g_sParams, &sNew, sizeof(g_sParams));
                g_bValid = true;
                bResult = true;
            }
        }
    }

    HAL_FLASH_Lock();

    return bResult;
}
