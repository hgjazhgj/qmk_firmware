# 本人自用的键盘固件

我有一块NuPhy Halo 75键盘，当初买它就是看中它提供了完整的QMK驱动源码，包括侧边装饰灯带，以及通过串口驱动无线射频芯片  

> 我之所以提到完整二字，是因为有些号称开源的厂商只提供最小部分的源码，只管USB有线连接插上后能够输入，此事在qmk shamelist中亦有记载  
> <https://docs.qmk.fm/license_violations>  
> 如果你购买一块宣称支持VIA的键盘，那么其固件应当按照QMK所规定的方式开源  

当然我不可避免地会对其代码质量颇有微词，但是已经比那些我无法自行开发的高到不知到哪里去了  

我将NuPhy厂商提供的源码适配至最新的QMK官方repo，包括：

- Legacy GPIO calls were migrated to `gpio_*` APIs.
- EEPROM user-datablock calls now provide offset and length.
- The removed `keyboard_protocol` global is no longer written.
- Old `RGB_*` keycodes were migrated to current RGB Matrix `RM_*` keycodes.
- IS31FL3733 types, channel names, I2C address definitions, and pull-resistor constants use the current driver API.
- `keyboard.json` RGB coordinates are integers, and unused LED entries include the coordinates required by the current schema.
- constant name updates in `config.h` due to removed compatibility layer that translates.
- `MAC_TASK` and `MAC_CONSOLE` replaced to QMK official implementation.

然后加入了一些我想要的功能，包括：

- 新增一个好看的灯光效果
- 新增一个用于指示当前精确电量和连接状态的灯光效果
- 修改默认的keymap，包括原厂代码中被逻辑交换引脚的INS和DEL
- 修改休眠行为为只关闭灯光，以免从休眠恢复时重新初始化射频芯片导致前几个按下的按键被丢弃
- 修改按下BAT_SHOW的行为为临时切换到新增的光效，松开后恢复，而右上角指示灯的电量指示常时开启

我在wsl2中使用`util/docker_build.sh nuphy/halo75_v2/ansi:via`编译并在windows中使用QMK Toolbox刷写固件  

---

任何人都可以自行编译本仓库源码以供个人使用，编译后产物不得公开发布  
