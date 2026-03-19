# Wi-Fi Direct CLI Tool 

## 背景

研究Android端与PC端(主要是windows)是否可以进行基于p2p连接的wifi-direct无感连接、进行通信的可能性，核心优势是不占用当前已连接的wifi接口，让连接设备正常使用各自的网络.

<br>

## 工具特色

- 本项目是从[windows官方wifi direct sample的cpp部分](https://github.com/Microsoft/Windows-universal-samples/tree/main/Samples/WiFiDirect/cpp)中提取出逻辑功能，去除windows桌面化部分，旨在减轻工具对visual studio开发工具链的依赖，对新手友好.


- 本项目代码生成部分依赖于Ai-coding([vibe记录](vibe_history/vibe_v1.json))，具体的编译流程、环境配置由作者本人完成.   ~~大家要多多vibe啊！~~

<br>

## Get start

### 前置要求

- [ ] windows sdk
- [ ] Visual Studio Build Tools(主要是cl.exe编译器)


### 编译

本项目在命令行编译，即windows的powershell中；当然其他终端也行，可自行尝试

```shell
call "<your_vs_community_dir>\VC\Auxiliary\Build\vcvars64.bat" x64 # 进行vs develop tool环境配置
cl # 确认是否有输出
where cl # 确认架构是否对应自己的系统
cd <current_project>
build.bat
```
成功执行后会在当前目录生成工具：**WifiDirectCLI.exe**


### 使用

[使用说明](docs/USAGE.md)  

<br>

## 参考

[1] windows官方 universal-samples：https://github.com/Microsoft/Windows-universal-samples/tree/main/Samples/WiFiDirect  


[2] claude-4.6-Opus，基于[windows官方wifi direct sample的cpp部分](https://github.com/Microsoft/Windows-universal-samples/tree/main/Samples/WiFiDirect/cpp)进行vibe
