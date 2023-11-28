# MR 框架

 ----------

<!-- TOC -->

* [简介](#简介)
* [关键特性](#关键特性)
* [主要组成](#主要组成)
* [标准化设备接口](#标准化设备接口)
* [配置工具](#配置工具)
* [目录结构](#目录结构)

<!-- TOC -->

 ----------

# 简介

`MR` 框架是专为嵌入式系统设计的轻量级框架。充分考虑了嵌入式系统在资源和性能方面的需求。
通过提供标准化的设备管理接口，极大简化了嵌入式应用开发的难度，帮助开发者快速构建嵌入式应用程序。

框架为开发者提供了标准化的开启(`open`)、关闭(`close`)、控制(`ioctl`)、读(`read`)、写(`write`)
等接口。它将应用程序与底层硬件驱动进行解耦。应用程序无需了解驱动的实现细节。
当硬件发生改变时,只需要适配底层驱动,应用程序就可以无缝迁移到新硬件上。这大大提高了软件的可重用性和应对新硬件的可扩展性。

 ----------

![](E:\Workspace\Clion\mr-library\mr-library\document\picture\README.png)

 ----------

# 关键特性

- 标准化的设备访问接口
- 应用程序和驱动开发解耦
- 简化底层驱动和应用程序开发
- 轻量易上手，资源占用低
- 模块化设计，各部分解耦合并独立开发，极低的硬件迁移成本
- 支持在裸机环境和操作系统环境下使用

# 主要组成

- 设备框架：提供设备访问标准接口
- 内存管理：动态内存管理
- 工具：链表、队列、平衡树等常用数据结构
- 各类功能组件

 ----------

# 标准化设备接口

设备的所有操作都可通过以下接口实现：

| 接口                   | 描述      |
|:---------------------|:--------|
| mr_dev_register      | 注册设备    |
| mr_dev_open          | 打开设备    |
| mr_dev_close         | 关闭设备    |
| mr_dev_ioctl         | 控制设备    |
| mr_dev_read          | 从设备读取数据 |
| mr_dev_write         | 向设备写入数据 |
| mr_dev_isr           | 设备中断控制  |
| mr_dev_get_full_name | 获取设备全称  |

 ----------

# 配置工具

`MR` 提供 `Kconfig` 可视化配置工具，开发者无需深入了解源代码即可进行配置。

`Kconfig` 会根据配置文件自动生成配置选项界面。开发者可以通过简单的操作来选择需要启用的功能组件和设置相关参数。

![](E:\Workspace\Clion\mr-library\mr-library\document\picture\README_Kconfig.png)

![](E:\Workspace\Clion\mr-library\mr-library\document\picture\README_Kconfig1.png)

通过修改参数，快速裁剪所需功能。配置完成后通过 `Python` 脚本自动生成配置文件。

 ----------

# 目录结构

| 名称         | 描述     |
|:-----------|:-------|
| bsp        | 板级支持包  |
| components | 组件     |
| device     | 设备文件   |
| document   | 文档     |
| driver     | 驱动文件   |
| include    | 库头文件   |
| source     | 库源文件   |
| Kconfig    | 配置文件   |
| kconfig.py | 自动配置脚本 |
| LICENSE    | 许可证    |

 ----------