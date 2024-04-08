
## 设计稿

见: [Datapath Trace and Telemetry方案设计](https://docs.geedge.net/pages/viewpage.action?pageId=124754302)

## 编译

目前只支持centos8。

需要先安装依赖库，见CI脚本。有些是公司内部的库，逐包下载安装是比较麻烦了，最好直接添加公司的yum源，直接找同事要个yum源文件即可。

```shell
mkdir build && cd build
cmake ..
make
```