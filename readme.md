# UCAS Kernel

该程序为中国科学院大学2020级本科操作系统研讨课作业，是一个微型的操作系统。

## 1. 使用说明

使用`make run`进入操作系统，`make debug`打开gdb远程调试端口

使用`make run-net`进入操作系统并且开启网卡驱动，使用`make debug-net`打开gdb远程调试端口并且开启网卡驱动

使用`make run`进入操作系统并开启双核，`make debug`打开gdb远程调试端口并开启双核

进入的界面都是自带的shell

## 2. 支持特性

* 系统调用
* Round Robin 进程调度
* 交互式shell
* 进程管理功能(kill, wait, exit...)
* 支持多核
* 支持网卡驱动(e1000)
* 微型文件系统

## 3. shell

shell目前支持的命令有：

```
clear ps exec kill mkfs statfs mkdir ls cd rmdir touch cat ln rm
```

## 4. 测试程序

测试程序可以运行在该内核上，具体说明参见`reame-test.md`

## 5.  关于qemu

qemu由中国科学院大学操作系统研讨课教学团队编写，目前绑定在虚拟机中。再过一段时间，虚拟机将会上传，此时将会更新readme。