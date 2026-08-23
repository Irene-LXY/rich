# A7 Query / A18 Step 根本功能代码

本目录是项目实际编译使用的功能模块，不是代码副本。

## 文件

- `monopoly/query.h`：Query数据视图与格式化接口。
- `query_format.c`：资金、点数、房产、道具和状态剩余轮数的输出实现。
- `query_command.c`：Query命令参数校验及运行时调用。
- `monopoly/step.h`：Step参数解析接口与错误类型。
- `step_command.c`：Step n参数解析及遥控骰子运行时调用。

## 公共集成点

以下文件同时服务其他功能，因此仍保留在项目公共目录：

- `include/monopoly/command.h`：注册Query和Step命令入口。
- `include/monopoly/runtime.h`：声明资产更新和遥控移动接口。
- `src/commands/command_dispatcher.c`：统一命令分发。
- `src/runtime/runtime.c`：连接玩家、回合、地图与本模块。

CMake已将本目录加入头文件搜索路径，并直接编译本目录下的三个 `.c` 文件。
