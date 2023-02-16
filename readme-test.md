# OSLAB 测试说明

## 1. BootLoader

Project 1 中的OS只支持批处理程序，测试程序在特性较多的内核上无法运行，这里不再介绍。

## 2. Simple Kernel

Project 2 以及之前的 Project 1的测试程序都是运行在比较原始的操作系统(运行在裸机上，没有虚拟内存，没有shell)，因此可能有些测试程序无法在特性较多的操作系统上正常运行。下面的测试结果是在原始的操作系统上得到的。

```
> [TASK] This task is to test scheduler. (459)                     
> [TASK] This task is to test scheduler. (413)                     
> [TASK] Applying for a lock                                                   
> [TASK] Has acquired lock and running.(4)                         
> [TASK] This task is to test sleep. (9)                           
> [TASK] This is a thread to timing! (12/123721200 seconds).       
> [TASK] Main thread: waiting for the 2 sub threads to finish.     
> [TASK] Thread 1 is running, sum1: 76636                          
> [TASK] Thread 2 is running, sum2: 435896                         
                                                                   
                                                  _                
                                                -=\`\              
                                            |\ ____\_\__           
                                          -=\c`""""""" "`)         
                                            `~~~~~/ /~~`           
                                              -==/ /               
                                                
```

第1, 2个task对应`print1.c`, `print2.c`，来测试调度器是否能够正常运行；第3, 4个task对应`lock1.c`, `lock2.c`，用来测试互斥锁是否被正确地实现；第5个task用来测试系统调用`sys_sleep`是否实现正确；第6个task用来测试关于时间的系统调用；第7, 8, 9个task用来测试线程是否正确；

## 3. Interactive OS and Process Management

### 3.1 waitpid

`waitpid.c`, `wait_locks.c`, `ready_to_exit.c` 三个文件构成了一个测试，用来检验系统调用`sys_exec`, `sys_waitpid`, `sys_kill` 是否实现正确。

在shell中运行`waitpid`时，该程序会首先启动`ready_to_exit`获取两个互斥锁，并且会一直运行；之后又启动`wait_locks`，`wait_locks`会一直等待，直到`ready_to_exit`进程被杀死，释放锁后，才会继续运行。`waitpid`会等待这两个程序运行完成，才会运行，然后退出。

首先在shell中输入以下命令：

```
exec waitpid &
```

可以看到shell的输出为：

```
[TASK] I want to wait task (pid=3) to exit.                                   
[TASK] I am task with pid 3, I have acquired two mutex locks. (962)           
[TASK] I want to acquire mutex lock1 (handle=4). 
```

此后我们在shell中输入：

```
kill 3
```

来杀死`ready_to_exit`进程，之后shell的输出为：

```
[TASK] Task (pid=4) has exited.                                               
[TASK] I am task with pid 3, I have acquired two mutex locks. (2687)          
[TASK] I have acquired mutex lock2 (handle=6).
```

### 3.2 barrier

`barrier.c`, `test_barrier.c` 两个文件是用来测试同步原语`barrier`是否正确实现的。启动`barrier`之后，`barrier`会启动3个`test_barrier`程序。这三个程序会睡眠随机时间，这样就人为造成了三个程序的不同步；此后再使用内核的同步原语进行显式同步，如果同步成功，那么说明barrier的实现就没有什么问题了。

若测试通过，那么当三个程序不同步时，有的程序会显示

```
> [TASK] Ready to enter the barrier.(n)
```

表示该程序到达barrier处，而有的程序会显示

```
> [TASK] Exited barrier (n-1).(sleep t s)
```

表示程序尚未到达barrier。

不过到最后所有程序在barrier处同步时，会一起退出：

```
> [TASK] Exited barrier (9).(sleep 3 s) .                                       
> [TASK] Exited barrier (9).(sleep 1 s)                                         
> [TASK] Exited barrier (9).(sleep 3 s)   
```

### 3.3 conditional variable

`condition.c`, `consumer.c`, `producer.c` 三个文件构成了一个生产者-消费者模型，用来测试同步原语`conditional variable`的实现是否正确。`condition`会启动三个`producer`程序，三个`consumer`程序，它们之间使用内核提供的`conditional variable`进行同步。

若测试通过，则输出如下所示：

```
> [TASK] Total produced 12 products. (next in 3 seconds)                        
> [TASK] Total consumed 4 products. (Sleep 1 seconds)                           
> [TASK] Total consumed 4 products. (Sleep 2 seconds)                           
> [TASK] Total consumed 4 products. (Sleep 1 seconds) 
```

producer行的数字与3个consumer行的数字之和应该是相同的。

测试程序是在Project3中使用的，进程之间互相访问彼此的内存；但是在Project4之后，虚拟内存开启，因此在开启虚拟内存的内核上运行上述程序，测试不会通过，需要读者自行修改测试程序。

### 3.4 mailbox

`mbox_client.c`, `mbox_server.c` 两个文件用来测试mailbox系统调用，分别是发送和接受。

若测试通过，则输出如下：

```
[Server] recved msg from 4 (blocked: 2, correctBytes: 242, errorBytes: 0)       
[Client] send bytes: 166, blocked: 0                                            
[Client] send bytes: 94, blocked: 0
```

client 发送的字节数，应该多于server的字节数，而且不能多太多，不能超过内核中mailbox缓冲区的长度。

测试程序是在Project3中使用的，进程之间互相访问彼此的内存；但是在Project4之后，虚拟内存开启，因此在开启虚拟内存的内核上运行上述程序，测试不会通过，需要读者自行修改测试程序。

### 3.5 multicore

`add.c` 和 `multicore.c` 两个文件构成了对系统多核性能的测试。

若测试通过，则输出如下：

```
start compute, from = 0, to = 2500000  Done                        
start compute, from = 2500000, to = 5000000  Done                  
                                                                   
                                                                   
                                                                   
single core: 4869453 ticks, result = 630                           
multi core: 2930418 ticks, result = 630  
```

单核所用的 tick 数应该是非常接近多核 tick 数的两倍。

## 4. Virtual Memory

## 4.1 rw.c

`rw.c`是用来检测虚拟内存系统是否能够完成按需调页的，也就是说当访问某个地址缺页时，内核应该为该虚拟地址分配一个内存页。

`rw.c`会接受命令行参数，向虚地址中写入一个随机数，再从该虚地址中读出这个数，检查这两个数是否相同。

使用方法如下：

```
exec rw 0x10800000 0x80200000 0xa0000320
```

若测试通过，则会打印写入和读出的随机数，并输出success.

### 4.2 swap.c

`swap.c`使用来检测虚拟内存系统的swap机制的。在使用该程序进行测试时，需要设定内存中PRESENT_PAGES的最大数量，也就是宏`MAX_PRESENT_PFN`(mm.h)的值为一个比较小的值，比如16，此后再执行程序rw。

程序会向连续64个页中写入数据，之后再从这64个页中读出数据，由于设定了`MAX_PRESENT_PFN`，在执行时会看到输出换页的信息。由于程序输出的信息太多，可以在调试模式下，一点一点观察程序的输出。

程序会输出写入的地址和值，以及换页信息，此时可以观察程序输出的信息是否与预期一致。

### 4.3 mailbox_thread.c

`mailbox_thread.c`用来测试多线程的mailbox收发。三个进程分别收发mail，每个进程分别执行发送mail和接受mail的操作。

执行测试：在shell中输入以下命令：

```
exec mailbox_thread a &
exec mailbox_thread b &
exec mailbox_thread c &
```

shell的输出应该与下面类似，经过一段时间后，12项中每一项都不为0

```
[a-send] to b: 212   to c: 54                                                  
[a-recv] from b: 23   from c: 13                                               
[b-send] to a: 44   to c: 58                                                   
[b-recv] from a: 101   from c: 7                                               
[c-send] to a: 36   to b: 11                                                   
[c-recv] from a: 29   from b: 21
```

### 4.4 consensus.c

`consensus.c`用来测试多进程共享内存系统调用`sys_shmpageget`和`sys_shmpagedt`是否实现正确。

若测试通过，shell会首先输出选中的进程，之后所有进程到达都到达barrier，会输出所有进程ready；此后每轮会选中一个进程，shell每次都会输出选中进程的序号；此后当所有进程被选中之后，程序退出。

## 5. Device Driver

### 5.1 send.c

`send.c`是用来测试OS是否能正常发送数据包的。使用`make run-net`运行内核，在命令行中执行`exec send`，然后在另外一个终端窗口中执行`sudo tcpdump -i tap0 -XX -vvv -nn`，监听来自内核的数据包。

若测试通过，则在执行send之后，tcpdump会接收到以下数据包：

```
tcpdump: listening on tap0, link-type EN10MB (Ethernet), capture size 262144 bytes
12:43:27.465268 IP truncated-ip - 138 bytes missing! (tos 0x0, ttl 255, id 0, offset 0, flags [DF], proto UDP (17), length 212)
    192.168.1.1.5353 > 224.0.0.251.5353: [no cksum] 0+ [251a] [24064q] [35n] [35826au][|domain]
        0x0000:  ffff ffff ffff 0055 7db5 7df7 0800 4500  .......U}.}...E.
        0x0010:  00d4 0000 4000 ff11 d873 c0a8 0101 e000  ....@....s......
        0x0020:  00fb 14e9 14e9 0400 0000 0000 0100 5e00  ..............^.
        0x0030:  00fb 0023 8bf2 b784 0800 4500 00d4 0000  ...#......E.....
        0x0040:  4000 ff11 d873 c0a8 0101 e000 00fb 14e9  @....s..........
        0x0050:  14e9 0108 0000 0000
```

不止上面的一个数据包，一共有四个数据包。

### 5.2 recv.c

`recv.c`是用来测试OS是否能接受数据包的。使用`make run-net`运行内核，在命令行中执行`exec recv`，然后在另外一个窗口中运行小程序`pktRxTx`, 执行`sudo ./pktRxTx -m 1`，选择`tap0`，根据提示发送数据包：

若测试成功，则执行recv后，shell的输出应该如下：

```
> [INIT] SCREEN initialization succeeded.                                       
[RECV] start recv(32): 2390, iteration = 1                                      
packet 31:                                                                      
00 0a 35 00 1e 53 80 fa 5b 33 56 ef 08 00 45 00                                 
00 39 d4 31 40 00 40 06 5c 4b 0a 00 00 43 ff ff                                 
ff ff b7 52 e5 40 00 00 02 05 00 00 00 00 50 18                                 
ff ff 4c 11 00 00 52 65 71 75 65 73 74 73 3a 20                                 
33 31 28 5e 5f 5e 29 00 00 00 00
```

## 6. File System

### 6.1 lseek.c

`lseek.c` 是用来测试文件系统是否支持多级页表和大文件读写的程序。测试程序会在大文件`8MB.dat`的开头写入一段文字，调用lseek之后在`8MB.dat`的最后写入一段文字。之后再从`8MB.dat`中读出写入的文字。

若测试通过，则会在shell中输出下面两行文字：

```
string at the end of the file.
string at the start of the file.
```

### 6.2 rwfile.c

`rwfile.c`是用来测试系统调用`sys_fopen`, `sys_fclose`, `sys_fread`, `sys_fwrite`,  `sys_lseek` 是否实现正确的程序。程序首先打开文件`1.txt`，向其中写入十行`hello world!`，此后在读出写入的十行`hello world!`，并打印出来，最后关闭文件`1.txt`。

若测试通过，则会在shell中输出10行 `hello world`.
