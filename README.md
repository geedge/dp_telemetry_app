
## 文档

1. [Datapath Trace and Telemetry方案设计](https://docs.geedge.net/pages/viewpage.action?pageId=124754302)
2. [Datapath Telemetry Job -- maat字段](https://docs.geedge.net/display/TSG/Datapath+Telemetry+Job)
3. [Datapath Telemetry Record -- kafka发送内容](https://docs.geedge.net/pages/viewpage.action?pageId=129088240)
4. [telemetry入门指北](https://docs.geedge.net/pages/viewpage.action?pageId=129100805)

## 编译

目前只支持centos8。

需要先安装依赖库，见CI脚本。有些是公司内部的库，逐包下载安装是比较麻烦了，最好直接添加公司的yum源，直接找同事要个yum源文件即可。

```shell
mkdir build && cd build
cmake ..
make
```

## 运行

拷贝etc目录到build目录。并根据实际情况，修改配置文件。

```shell
# build dir
./src/dp_trace_telemetry -c etc/dp_trace.conf -d etc/dp_trace_dy.conf
```