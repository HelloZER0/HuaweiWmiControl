/* ============================================================================
 * wmi.cpp — COM/WMI 连接、协议探测、命令调用（核心层）
 * ----------------------------------------------------------------------------
 * 只用 Windows SDK + IWbemLocator/COM API：
 *   - 不使用 ATL / WIL / _bstr_t / comdef.h / comsuppw；
 *   - BSTR 一律 SysAllocString/SysFreeString；
 *   - VARIANT 一律 VariantInit/VariantClear；
 *   - SAFEARRAY 交给 VARIANT 托管（VariantClear 会 SafeArrayDestroy 一次）。
 *
 * 异常约束：本文件不抛异常、不使用会抛异常的 C++ 设施，所有失败都折成
 * HwStatus 返回；COM 指针/字符串全部用 RAII 小结构体托管，early-return
 * 不会泄漏。
 * ========================================================================== */

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601 /* Windows 7+：TokenElevation 等 */
#endif

#include <windows.h>
#include <objbase.h>
#include <oleauto.h>
#include <wbemidl.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "hwapi.h"

/* ---------------------------------------------------------------------------
 * 供 services.cpp 复用的内部入口（非 hwapi.h 公开 API）。
 * 两个 TU 中的声明必须保持一致；定义带 extern "C" 以固定链接。
 * ------------------------------------------------------------------------- */
extern "C" HwStatus hw_wmi_call(HwDevice* dev, uint64_t cmd,
                                uint8_t* out, int* out_len);

namespace {

/* ------------------------------------------------ 内部连接状态（dev->_p） */
struct HwWmiConn {
    IWbemServices*    pSvc;      /* ROOT\WMI 命名空间代理            */
    IWbemClassObject* pClass;    /* OemWMIMethod 类定义（缓存）      */
    int               com_inited;/* 1 = 本对象需要 CoUninitialize    */
};

/* ------------------------------------------------------------- RAII 工具 */

template <class T>
struct ScopedCom {
    T* p;
    ScopedCom() : p(NULL) {}
    ~ScopedCom() { reset(); }
    T* detach() { T* q = p; p = NULL; return q; }
    void reset()
    {
        if (p) {
            p->Release();
            p = NULL;
        }
    }
private:
    ScopedCom(const ScopedCom&);
    ScopedCom& operator=(const ScopedCom&);
};

struct ScopedBstr {
    BSTR b;
    ScopedBstr() : b(NULL) {}
    ~ScopedBstr() { clear(); }
    void clear()
    {
        if (b) {
            SysFreeString(b);
            b = NULL;
        }
    }
    int alloc(const WCHAR* s)
    {
        clear();
        if (s) b = SysAllocString(s);
        return b != NULL;
    }
    void attach(BSTR s) /* 接管一个已分配的 BSTR */
    {
        clear();
        b = s;
    }
private:
    ScopedBstr(const ScopedBstr&);
    ScopedBstr& operator=(const ScopedBstr&);
};

struct ScopedVariant {
    VARIANT v;
    ScopedVariant() { VariantInit(&v); }
    ~ScopedVariant() { VariantClear(&v); }
private:
    ScopedVariant(const ScopedVariant&);
    ScopedVariant& operator=(const ScopedVariant&);
};

/* COM 套间生命周期：只有确实由本对象 CoInitializeEx 成功时才 CoUninitialize。 */
struct ComApartment {
    int owned;
    ComApartment() : owned(0) {}
    ~ComApartment()
    {
        if (owned) {
            CoUninitialize();
            owned = 0;
        }
    }
private:
    ComApartment(const ComApartment&);
    ComApartment& operator=(const ComApartment&);
};

/* ----------------------------------------------------------------- 小工具 */

/* 有界截断复制（不用会触发无效参数处理器的 API）。 */
static void hwc_copy_w(WCHAR* dst, size_t cap, const WCHAR* src)
{
    if (!dst || cap == 0) return;
    dst[0] = L'\0';
    if (!src) return;
    wcsncpy_s(dst, cap, src, _TRUNCATE);
}

/* 有界追加，返回新的结束偏移。 */
static size_t hwc_append(WCHAR* dst, size_t cap, size_t off, const WCHAR* src)
{
    if (!dst || cap == 0) return 0;
    if (off >= cap) return cap - 1;
    while (src && *src != L'\0' && off + 1 < cap) dst[off++] = *src++;
    dst[off] = L'\0';
    return off;
}

/* 构造实例对象路径：OemWMIMethod.InstanceName='ACPI\PNP0C14\HWMI_0'
 * 真实固件要求按实例调用（见 docs/reverse-engineering.md），
 * 只有拿不到实例名时才退回类名。 */
static BSTR hwc_make_instance_path(const WCHAR* instance)
{
    WCHAR buf[320];
    buf[0] = L'\0';
    if (!instance || instance[0] == L'\0') return NULL;
    (void)hwc_append(buf, _countof(buf), 0, L"OemWMIMethod.InstanceName='");
    (void)hwc_append(buf, _countof(buf), wcslen(buf), instance);
    (void)hwc_append(buf, _countof(buf), wcslen(buf), L"'");
    return SysAllocString(buf);
}

/* 是否已提权（TokenElevation）。 */
static int hwc_is_admin(void)
{
    HANDLE token = NULL;
    TOKEN_ELEVATION elevation;
    DWORD ret_len = 0;
    int admin = 0;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return 0;
    ZeroMemory(&elevation, sizeof(elevation));
    if (GetTokenInformation(token, TokenElevation, &elevation,
                            sizeof(elevation), &ret_len))
        admin = (elevation.TokenIsElevated != 0) ? 1 : 0;
    CloseHandle(token);
    return admin;
}

/* 进程内 CoInitializeSecurity 只需成功一次；失败（含 RPC_E_TOO_LATE）不致命。 */
static volatile LONG g_sec_done = 0;

static void hwc_init_security_once(void)
{
    HRESULT hr;
    if (InterlockedCompareExchange(&g_sec_done, 1, 0) != 0) return;
    hr = CoInitializeSecurity(NULL, -1, NULL, NULL,
                              RPC_C_AUTHN_LEVEL_DEFAULT,
                              RPC_C_IMP_LEVEL_IMPERSONATE,
                              NULL, EOAC_NONE, NULL);
    /* RPC_E_TOO_LATE（已被其他组件设置）与其它失败都允许继续：
     * 本地 ROOT\WMI 在这两种情况下通常仍可连接。 */
    (void)hr;
}

/* ------------------------------------------------------ 实例枚举（步骤 g） */
/* 返回 1 并写入 name；优先取 InstanceName 含 "HWMI" 的实例，否则第一个。 */
static int hwc_find_instance(IWbemServices* svc, WCHAR* name, size_t cap)
{
    ScopedCom<IEnumWbemClassObject> enumerator;
    ScopedBstr bstr_lang;
    ScopedBstr bstr_query;
    WCHAR first[128];
    WCHAR hwmi[128];
    HRESULT hr;

    if (!svc || !name || cap == 0) return 0;
    name[0] = L'\0';
    first[0] = L'\0';
    hwmi[0] = L'\0';

    if (!bstr_lang.alloc(L"WQL")) return 0;
    if (!bstr_query.alloc(L"SELECT * FROM OemWMIMethod")) return 0;

    hr = svc->ExecQuery(bstr_lang.b, bstr_query.b,
                        (LONG)(WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY),
                        NULL, &enumerator.p);
    if (FAILED(hr) || !enumerator.p) return 0;

    for (;;) {
        IWbemClassObject* obj = NULL;
        ULONG returned = 0;

        hr = enumerator.p->Next((LONG)WBEM_INFINITE, 1, &obj, &returned);
        if (FAILED(hr) || returned == 0 || obj == NULL) {
            if (obj) obj->Release();
            break;
        }

        {
            ScopedVariant val;
            HRESULT hr2 = obj->Get(HW_WMI_INST_PROP, 0, &val.v, NULL, NULL);
            if (SUCCEEDED(hr2) && val.v.vt == VT_BSTR && val.v.bstrVal != NULL) {
                if (first[0] == L'\0')
                    hwc_copy_w(first, _countof(first), val.v.bstrVal);
                if (hwmi[0] == L'\0' &&
                    wcsstr(val.v.bstrVal, HW_WMI_INST_FILTER) != NULL)
                    hwc_copy_w(hwmi, _countof(hwmi), val.v.bstrVal);
            }
        }

        obj->Release();
        if (hwmi[0] != L'\0') break; /* 已命中首选实例 */
    }

    hwc_copy_w(name, cap, (hwmi[0] != L'\0') ? hwmi : first);
    return (name[0] != L'\0') ? 1 : 0;
}

/* ------------------------------------------------------ 协议探测（步骤 h） */
/* OemWMIfun 的入参签名里有 u64Input → U64；否则按老固件 U8 处理。 */
static HwProtocol hwc_probe_protocol(IWbemClassObject* klass)
{
    ScopedCom<IWbemClassObject> in_sig;
    HRESULT hr;
    HwProtocol proto = HW_PROTO_U8;

    if (!klass) return HW_PROTO_U8;

    hr = klass->GetMethod(HW_WMI_METHOD, 0, &in_sig.p, NULL);
    if (FAILED(hr) || !in_sig.p) return HW_PROTO_U8;

    hr = in_sig.p->BeginMethodEnumeration(0);
    if (FAILED(hr)) return HW_PROTO_U8;

    for (;;) {
        BSTR pname = NULL;
        hr = in_sig.p->NextMethod(0, &pname, NULL, NULL);
        if (FAILED(hr) || pname == NULL) break;
        if (_wcsicmp(pname, HW_WMI_IN_U64) == 0) proto = HW_PROTO_U64;
        SysFreeString(pname);
        if (proto == HW_PROTO_U64) break;
    }

    (void)in_sig.p->EndMethodEnumeration();
    return proto;
}

/* ---------------------------------------------------- 单次命令调用（核心） */
/* 成功返回 HW_OK（不代表固件成功，调用方需检查 buf[0]）；COM 层失败返回
 * HW_ERR_COM；输出不是 64 字节 UI1 数组时返回 HW_ERR_BUFFER。 */
static HwStatus hwc_call_once(HwDevice* dev, uint64_t cmd,
                              uint8_t* out, int* out_len)
{
    HwWmiConn* conn;
    ScopedCom<IWbemClassObject> in_sig;
    ScopedCom<IWbemClassObject> in_params;
    ScopedCom<IWbemClassObject> out_params;
    ScopedBstr bstr_method;
    ScopedBstr bstr_class;
    ScopedBstr bstr_path;
    ScopedVariant var_in;
    ScopedVariant var_out;
    HRESULT hr;

    if (!dev || !dev->_p || !out) return HW_ERR_COM;
    conn = static_cast<HwWmiConn*>(dev->_p);
    if (!conn->pSvc || !conn->pClass) return HW_ERR_COM;

    /* 1) 方法定义 → 入参模板 */
    hr = conn->pClass->GetMethod(HW_WMI_METHOD, 0, &in_sig.p, NULL);
    if (FAILED(hr) || !in_sig.p) return HW_ERR_COM;

    /* 2) 每次调用都必须新 SpawnInstance 一份入参对象 */
    hr = in_sig.p->SpawnInstance(0, &in_params.p);
    if (FAILED(hr) || !in_params.p) return HW_ERR_COM;

    /* 3) 填参 */
    if (dev->protocol == HW_PROTO_U64) {
        var_in.v.vt = (VARTYPE)VT_UI8;
        var_in.v.ullVal = cmd;
        hr = in_params.p->Put(HW_WMI_IN_U64, 0, &var_in.v, 0);
        if (FAILED(hr)) return HW_ERR_COM;
    } else {
        SAFEARRAYBOUND bound;
        SAFEARRAY* sa;

        bound.lLbound = 0;
        bound.cElements = (ULONG)HW_U8_BUFFER_LENGTH;
        sa = SafeArrayCreate(VT_UI1, 1, &bound);
        if (!sa) return HW_ERR_COM;

        /* 数组所有权立即交给 VARIANT：任何 early-return 都由 VariantClear
         * 释放，不会 SafeArrayDestroy 两次。 */
        var_in.v.vt = (VARTYPE)(VT_ARRAY | VT_UI1);
        var_in.v.parray = sa;

        {
            void* data = NULL;
            int i;
            hr = SafeArrayAccessData(sa, &data);
            if (FAILED(hr) || !data) return HW_ERR_COM;
            ZeroMemory(data, HW_U8_BUFFER_LENGTH);
            for (i = 0; i < HW_U8_CMD_BYTES; ++i) {
                static_cast<unsigned char*>(data)[i] =
                    (unsigned char)((cmd >> (8 * i)) & 0xFFu);
            }
            (void)SafeArrayUnaccessData(sa);
        }

        hr = in_params.p->Put(HW_WMI_IN_U8, 0, &var_in.v, 0);
        if (FAILED(hr)) return HW_ERR_COM;
    }

    /* 4) 执行：优先按实例对象路径（真实固件需要），失败退回类名。 */
    if (!bstr_method.alloc(HW_WMI_METHOD)) return HW_ERR_COM;

    hr = E_FAIL;
    if (dev->instance[0] != L'\0') {
        bstr_path.attach(hwc_make_instance_path(dev->instance));
        if (bstr_path.b != NULL) {
            hr = conn->pSvc->ExecMethod(bstr_path.b, bstr_method.b, 0, NULL,
                                        in_params.p, &out_params.p, NULL);
        }
    }
    if (FAILED(hr) || !out_params.p) {
        out_params.reset();
        if (!bstr_class.alloc(HW_WMI_CLASS)) return HW_ERR_COM;
        hr = conn->pSvc->ExecMethod(bstr_class.b, bstr_method.b, 0, NULL,
                                    in_params.p, &out_params.p, NULL);
    }
    if (FAILED(hr) || !out_params.p) return HW_ERR_COM;

    /* 5) 取 u8Output */
    hr = out_params.p->Get(HW_WMI_OUT_PARAM, 0, &var_out.v, NULL, NULL);
    if (FAILED(hr)) return HW_ERR_COM;
    if (var_out.v.vt != (VARTYPE)(VT_ARRAY | VT_UI1) || var_out.v.parray == NULL)
        return HW_ERR_BUFFER;

    {
        LONG lbound = 0;
        LONG ubound = 0;
        LONG count;
        void* data = NULL;

        if (FAILED(SafeArrayGetLBound(var_out.v.parray, 1, &lbound)))
            return HW_ERR_BUFFER;
        if (FAILED(SafeArrayGetUBound(var_out.v.parray, 1, &ubound)))
            return HW_ERR_BUFFER;

        count = ubound - lbound + 1;
        if (count <= 0) return HW_ERR_BUFFER;
        if (count > (LONG)HW_OUT_MAX) count = (LONG)HW_OUT_MAX;

        hr = SafeArrayAccessData(var_out.v.parray, &data);
        if (FAILED(hr) || !data) return HW_ERR_BUFFER;
        memcpy(out, data, (size_t)count);
        (void)SafeArrayUnaccessData(var_out.v.parray);

        if (out_len) *out_len = (int)count;
    }

    return HW_OK;
}

} /* namespace */

/* ==========================================================================
 * 对外内部入口：带一次重试（固件状态非零属预期，非 COM 故障）
 * out/out_len 可为 NULL —— 纯 SET 命令只关心状态码。
 * ========================================================================== */
extern "C" HwStatus hw_wmi_call(HwDevice* dev, uint64_t cmd,
                                uint8_t* out, int* out_len)
{
    uint8_t tmp[HW_OUT_MAX];
    int attempt;

    if (!dev) return HW_ERR_ARG;
    if (!dev->available || !dev->_p) return HW_ERR_UNKNOWN;

    for (attempt = 0; attempt < 2; ++attempt) {
        int n = 0;
        HwStatus st = hwc_call_once(dev, cmd, tmp, &n);

        if (st != HW_OK) return st; /* COM 层故障：重试无意义 */

        if (n > 0 && tmp[0] == (uint8_t)HW_STATUS_SUCCESS) {
            /* 成功时清零，避免诊断把上一次失败的状态码当成本次结果。 */
            dev->last_fw_status = 0;
            if (out) memcpy(out, tmp, (size_t)n);
            if (out_len) *out_len = n;
            return HW_OK;
        }

        if (n > 0)     dev->last_fw_status = (int)tmp[0];
        else           dev->last_fw_status = (int)HW_STATUS_DEFAULT_ERROR;
    }

    return HW_ERR_NOT_SUPPORTED;
}

/* ==========================================================================
 * 生命周期
 * ========================================================================== */

void hw_device_init(HwDevice* dev)
{
    if (!dev) return;
    ZeroMemory(dev, sizeof(*dev));
    dev->protocol = HW_PROTO_UNKNOWN;
    dev->kbd_encoding = HW_KBD_UNKNOWN;
}

HwStatus hw_open(HwDevice* dev)
{
    ComApartment com;
    ScopedCom<IWbemLocator> locator;
    ScopedCom<IWbemServices> services;
    ScopedCom<IWbemClassObject> klass;
    ScopedBstr bstr_ns;
    ScopedBstr bstr_class;
    HwWmiConn* conn = NULL;
    WCHAR inst_name[128];
    HwProtocol proto = HW_PROTO_U8;
    HRESULT hr;

    if (!dev) return HW_ERR_ARG;

    /* 幂等：先关掉上一次的连接 */
    hw_close(dev);
    dev->available = 0;
    dev->protocol = HW_PROTO_UNKNOWN;
    dev->kbd_encoding = HW_KBD_UNKNOWN;
    dev->last_fw_status = 0;
    dev->instance[0] = L'\0';
    dev->reason[0] = L'\0';

    /* a) 管理员检查 */
    if (!hwc_is_admin()) {
        hwc_copy_w(dev->reason, _countof(dev->reason),
                   L"需要管理员权限：请右键选择「以管理员身份运行」。");
        return HW_ERR_NOT_ADMIN;
    }

    /* b) COM 初始化（RPC_E_CHANGED_MODE 时沿用线程已有的套间模型） */
    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (hr == RPC_E_CHANGED_MODE) {
        com.owned = 0;
    } else if (FAILED(hr)) {
        hwc_copy_w(dev->reason, _countof(dev->reason),
                   L"COM 初始化失败（CoInitializeEx），无法访问 WMI。");
        return HW_ERR_COM;
    } else {
        com.owned = 1;
    }

    /* c) 进程级安全设置：只做一次 */
    hwc_init_security_once();

    /* d) IWbemLocator */
    hr = CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
                          IID_IWbemLocator,
                          reinterpret_cast<LPVOID*>(&locator.p));
    if (FAILED(hr) || !locator.p) {
        hwc_copy_w(dev->reason, _countof(dev->reason),
                   L"创建 IWbemLocator 失败（WMI 服务可能已停止）。");
        return HW_ERR_COM;
    }

    /* e) 连接 ROOT\WMI */
    if (!bstr_ns.alloc(HW_WMI_NAMESPACE)) {
        hwc_copy_w(dev->reason, _countof(dev->reason),
                   L"内存不足：无法构造 WMI 命名空间名。");
        return HW_ERR_COM;
    }
    hr = locator.p->ConnectServer(bstr_ns.b, NULL, NULL, NULL, 0, NULL, NULL,
                                  &services.p);
    if (FAILED(hr) || !services.p) {
        hwc_copy_w(dev->reason, _countof(dev->reason),
                   L"连接 ROOT\\WMI 命名空间失败（请确认以管理员身份运行）。");
        return HW_ERR_COM;
    }

    /* f) 代理安全级别 */
    hr = CoSetProxyBlanket(services.p, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                           NULL, RPC_C_AUTHN_LEVEL_CALL,
                           RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
    if (FAILED(hr)) {
        hwc_copy_w(dev->reason, _countof(dev->reason),
                   L"设置 WMI 代理安全级别失败（CoSetProxyBlanket）。");
        return HW_ERR_COM;
    }

    /* g) 枚举 OemWMIMethod 实例，优先含 "HWMI" 的那个 */
    if (!hwc_find_instance(services.p, inst_name, _countof(inst_name))) {
        hwc_copy_w(dev->reason, _countof(dev->reason),
                   L"未找到 OemWMIMethod 实例（驱动未安装或非华为机型）。");
        return HW_ERR_NO_DEVICE;
    }

    /* h) 取类定义（缓存给后续调用）并探测协议 */
    if (!bstr_class.alloc(HW_WMI_CLASS)) {
        hwc_copy_w(dev->reason, _countof(dev->reason),
                   L"内存不足：无法构造类名。");
        return HW_ERR_COM;
    }
    hr = services.p->GetObject(bstr_class.b, 0, NULL, &klass.p, NULL);
    if (FAILED(hr) || !klass.p) {
        hwc_copy_w(dev->reason, _countof(dev->reason),
                   L"无法读取 OemWMIMethod 类定义。");
        return HW_ERR_COM;
    }
    proto = hwc_probe_protocol(klass.p);

    /* i) 组装连接状态；成功后 COM 生命周期移交给 dev */
    conn = static_cast<HwWmiConn*>(calloc(1, sizeof(HwWmiConn)));
    if (!conn) {
        hwc_copy_w(dev->reason, _countof(dev->reason),
                   L"内存不足：无法分配连接状态。");
        return HW_ERR_COM;
    }
    conn->pSvc = services.detach();
    conn->pClass = klass.detach();
    conn->com_inited = com.owned;

    com.owned = 0; /* 不再由 ComApartment 做 CoUninitialize */

    dev->_p = conn;
    dev->protocol = proto;
    dev->available = 1;
    hwc_copy_w(dev->instance, _countof(dev->instance), inst_name);

    /* j) 键盘背光编码探测：失败只意味着编码未知，连接仍算成功 */
    {
        uint8_t buf[HW_OUT_MAX];
        int n = 0;
        dev->kbd_encoding = HW_KBD_UNKNOWN;
        if (hw_wmi_call(dev, hw_cmd_simple(HW_C_KBDLIGHT_GET), buf, &n) == HW_OK)
            dev->kbd_encoding = hw_kbd_classify(buf, n);
        dev->last_fw_status = 0;
    }

    return HW_OK;
}

void hw_close(HwDevice* dev)
{
    HwWmiConn* conn;

    if (!dev) return;

    conn = static_cast<HwWmiConn*>(dev->_p);
    if (conn) {
        dev->_p = NULL;
        if (conn->pClass) {
            conn->pClass->Release();
            conn->pClass = NULL;
        }
        if (conn->pSvc) {
            conn->pSvc->Release();
            conn->pSvc = NULL;
        }
        if (conn->com_inited) {
            CoUninitialize();
            conn->com_inited = 0;
        }
        free(conn);
    }

    dev->available = 0;
    dev->protocol = HW_PROTO_UNKNOWN;
    dev->kbd_encoding = HW_KBD_UNKNOWN;
}
