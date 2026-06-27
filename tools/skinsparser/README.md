# TTPlayer SkinsParser

千千静听 `.skn` 皮肤文件的解析/提取工具。

## 功能

| 命令 | 说明 |
|------|------|
| `list` | 列出 .skn 内的所有文件 |
| `extract` | 解压 .skn 到目录 |
| `info` | 显示皮肤基本信息（名称、作者、版本等） |
| `map` | **显示完整的 UI 元素映射表**（UI 控件 -> BMP 文件） |

## 使用示例

```bash
# 列出 .skn 内容
ttplayer-skinsparser list "经典皮肤.skn"

# 解压到当前目录
ttplayer-skinsparser extract "经典皮肤.skn"

# 解压到指定目录
ttplayer-skinsparser extract "经典皮肤.skn" ./output/

# 查看皮肤信息
ttplayer-skinsparser info "经典皮肤.skn"
ttplayer-skinsparser info ./解压后的目录

# 查看完整的 UI 元素映射表 ★ 最常用
ttplayer-skinsparser map "经典皮肤.skn"
```

## 输出示例 (map 命令)

```
=== 映射摘要 (ttplayer UI -> 图片文件) ===

  播放按钮 (play)          -> play.bmp           [位置: x=8 y=125 size=30x30]
  暂停按钮 (pause)          -> pause.bmp          [位置: x=8 y=125 size=30x30]
  进度条 (progress)         -> progress_thumb.bmp [位置: x=18 y=106 size=230x11]
  音量滑块 (volume)         -> volume_thumb.bmp + volume_fill.bmp
  关闭/退出按钮 (exit)      -> exit.bmp           [位置: x=245 y=6 size=15x15]
  ...
```

## 依赖

- Qt5 或 Qt6 (Core 模块)
- [QuaZip](https://github.com/stachenov/quazip) (ZIP 压缩/解压库)

## 构建

```bash
cd tools/skinsparser
mkdir build && cd build
cmake .. -DQUAZIP_ROOT=/path/to/quazip
make
```

## .skn 文件格式说明

`.skn` 文件本质上是标准 **ZIP 压缩包**，内部结构：

```
xxx.skn  (ZIP 格式)
├── Skin.xml          ← 核心配置：定义所有 UI 元素的坐标和图片映射
├── Lyric.xml         ← 歌词样式（字体、颜色）
├── PlayList.xml      ← 播放列表样式
├── Visual.xml        ← 频谱可视化颜色方案
├── player_skin.bmp   ← 主界面背景图
├── play.bmp          ← 播放按钮 (四宫格: 正常/悬停/按下/禁用)
├── pause.bmp         ← 暂停按钮
├── ...
└── *.bmp             ← 其他 UI 元素图片
```

### Skin.xml 结构示例

```xml
<skin version="2" name="经典皮肤" transparent_color="#ff00ff">
  <player_window image="player_skin.bmp">
    <play position="8,125,38,155" image="play.bmp" />
    <pause position="8,125,38,155" image="pause.bmp" />
    <stop position="43,130,63,150" image="stop.bmp" />
    <prev position="70,130,90,150" image="prev.bmp" />
    <next position="95,130,115,150" image="next.bmp" />
    <progress position="18,106,248,117" thumb_image="progress_thumb.bmp" />
    <volume position="151,130,217,148" thumb_image="volume_thumb.bmp" fill_image="volume_fill.bmp" />
    <!-- ... 更多元素 ... -->
  </player_window>
  <lyric_window ...>
  <equalizer_window ...>
  <playlist_window ...>
</skin>
```
