cmake_minimum_required(VERSION 3.20)

project(HuaweiWmiControlNative
        VERSION 1.0.0
        DESCRIPTION "华为/荣耀笔记本调控工具（原生 Win32 版）"
        LANGUAGES C CXX RC)

# ---- 工具链要求 -------------------------------------------------------------
# 只用 Windows SDK 自带头文件，不依赖任何第三方库（无 ATL / 无 WIL / 无 .NET）。
if(NOT MSVC)
    message(FATAL_ERROR
        "本项目仅针对 MSVC 工具链编写（依赖 IWbemLocator、Shell_NotifyIcon 与 "
        "WIN32 子系统链接脚本）。请用 Visual Studio 2022 / Build Tools 2022 构建。")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(WARNING "建议以 x64 构建：当前指针宽度 ${CMAKE_SIZEOF_VOID_P} 字节。")
endif()

set(CMAKE_C_STANDARD   11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- 源文件 ---------------------------------------------------------------
add_executable(HuaweiWmiControl WIN32
    # 契约层（Lead 维护）
    src/proto.c
    src/proto.h
    src/hwapi.h
    # 核心层（COM/WMI）
    src/log.cpp
    src/wmi.cpp
    src/services.cpp
    # Win32 外壳层
    src/icon.cpp
    src/tray.cpp
    src/menu.cpp
    src/main.cpp
    # 资源（版本信息）+ 清单由链接器嵌入
    app.rc
)

target_include_directories(HuaweiWmiControl PRIVATE src)

target_compile_definitions(HuaweiWmiControl PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    # 去掉「不是所有控件都初始化」等无关告警噪音
    _CRT_SECURE_NO_WARNINGS
)

target_compile_options(HuaweiWmiControl PRIVATE
    /utf-8          # 源码与执行字符集均为 UTF-8（中文菜单必需）
    /W4
    /permissive-
)

# ---- 清单：requireAdministrator + PerMonitorV2 ------------------------------
# 用链接器嵌入而不是 .rc 的 RT_MANIFEST，避免与 MSVC 默认清单冲突。
# /MANIFESTUAC:NO 是必需的：否则链接器仍会生成一份 level="asInvoker" 的
# UAC 片段，与本文件里的 requireAdministrator 合并时报
# "manifest authoring error c1010001: Values of attribute level not equal"。
target_link_options(HuaweiWmiControl PRIVATE
    "/MANIFEST:EMBED"
    "/MANIFESTINPUT:${CMAKE_CURRENT_SOURCE_DIR}/app.manifest"
    "/MANIFESTUAC:NO"
)

# ---- 链接库 ---------------------------------------------------------------
# wbemuuid: CLSID_WbemLocator / IID_IWbemLocator
# ole32/oleaut32: BSTR / VARIANT / SAFEARRAY
# shlwapi: PathRemoveFileSpec 等路径工具
# comctl32: 菜单/托盘相关的 InitCommonControlsEx
target_link_libraries(HuaweiWmiControl PRIVATE
    wbemuuid
    ole32
    oleaut32
    user32
    shell32
    shlwapi
    comctl32
    gdi32
    advapi32
)

# ---- 静态 CRT：生成的 exe 不依赖 VC++ 运行时可再分发 ------------------------
set_property(TARGET HuaweiWmiControl PROPERTY
             MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ==== 纯逻辑测试 =============================================================
# proto.h 全部是纯函数（不碰 Windows.h / COM / 硬件），因此这一组测试
# 可以在没有华为设备、甚至没有 Windows 的机器上跑——CI 里唯一有意义的那部分。
option(BUILD_PROTO_TESTS "构建 proto.h 纯逻辑测试" ON)

if(BUILD_PROTO_TESTS)
    add_executable(test_proto
        tests/test_proto.c
        src/proto.c
    )
    target_include_directories(test_proto PRIVATE src)
    target_compile_options(test_proto PRIVATE /utf-8 /W4)
    set_property(TARGET test_proto PROPERTY
                 MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
