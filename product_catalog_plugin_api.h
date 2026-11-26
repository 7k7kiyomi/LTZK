#pragma once

#include <stdint.h>

#if defined(_WIN32) && defined(LTZK_PRODUCT_CATALOG_PLUGIN_BUILD)
#define LTZK_PRODUCT_CATALOG_API __declspec(dllexport)
#else
#define LTZK_PRODUCT_CATALOG_API
#endif

// 跨平台主程序与成果管理插件之间的稳定 C ABI 请求结构。
// 不在接口中传递 QString、QVariant、QObject 或插件内部类，避免 Qt/C++ ABI
// 和对象生命周期跨模块耦合。
#define LTZK_PRODUCT_CATALOG_API_VERSION 1u

typedef struct LTZKProductCatalogOperationRequest
{
    uint32_t apiVersion;
    const char* operationUtf8;
    int32_t directoryId;
    int32_t productId;
    const int32_t* productIds;
    uint32_t productIdCount;
    const char* connectionJsonUtf8;
} LTZKProductCatalogOperationRequest;

enum LTZKProductCatalogApiResult
{
    LTZK_PRODUCT_CATALOG_ACCEPTED = 0,
    LTZK_PRODUCT_CATALOG_QUEUED = 1,
    LTZK_PRODUCT_CATALOG_INVALID_ARGUMENT = -1,
    LTZK_PRODUCT_CATALOG_NOT_INITIALIZED = -2,
    LTZK_PRODUCT_CATALOG_UNSUPPORTED_OPERATION = -3,
    LTZK_PRODUCT_CATALOG_DISPATCH_FAILED = -4
};

#ifdef __cplusplus
extern "C"
{
#endif

typedef int (*LTZKProductCatalogOpenOperationFn)(
    const LTZKProductCatalogOperationRequest* request);

// DLL 导出函数。平台侧通过 QLibrary::resolve 获取地址，避免链接阶段依赖
// 插件 import library。
LTZK_PRODUCT_CATALOG_API int ltzk_product_catalog_open_operation(
    const LTZKProductCatalogOperationRequest* request);

#ifdef __cplusplus
}
#endif
