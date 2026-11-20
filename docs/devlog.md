# 开发日志

> 每天一句：今天解决了什么、卡在哪。这份日志也是面试时的"项目经历"素材。

## D0 · 2026-11-04 —— 环境与骨架

**决定**：接手时工作目录里已有一份单文件实现（约 700 行，把光栅化、深度、透视校正、
SSAA 全塞在 `src/main.cpp` 里）。它不符合指导 §5 的架构，也不符合"一天一天搭起来"的节奏，
所以按指导从 D0 重做。那份代码留在第一个提交里做存档，不参与后续构建。

**做了**：

- 按 §3.1 建目录：`math/ core/ pipeline/ scene/ platform/`、`tests/`、`bench/`、`assets/`、`docs/images/`
- 按 §3.2 把 CMake 拆成两个 target：`sr_core`（纯 C++，不依赖 SDL）+ `soft_raster`（SDL 只出现在 `platform/`）
- `Framebuffer`：颜色 `0xAARRGGBB` + 深度 `[0,1]`，带 `clear` / `setPixel` / `exportTGA`
- `Window`：SDL2 开窗、事件轮询（关窗 / ESC / S）、`SDL_UpdateTexture` + `SDL_RenderCopy` 上屏
- 屏幕上显示渐变，并把同一帧导出成 `out.tga`

**验证**：

```text
$ ./build/soft_raster
已导出 out.tga
$ file out.tga
out.tga: Targa image data - RGB 960 x 540 x 24 - top
$ python -c "…"     # 用 Pillow 解码校对
corner(0,0)=(0,0,128)   corner(959,539)=(255,255,128)
```

两个角点颜色说明 y 没有翻转、RGBA 通道顺序也没有写错——这类"用数据验方向"的习惯从 D0 就开始。

**卡点**：

- `-Wall -Wextra` 报了 `m_height` 未使用，顺手在 `present()` 里加了尺寸一致性检查（纹理尺寸是建时定死的）。
- TGA 的 18 字节头里，第 17 字节（image descriptor）的 bit 5 决定原点在左上还是左下。
  不置位的话图像会上下颠倒，而且**肉眼看起来像是"渲染错了"**，实际上是存图错了。

**下一步（D1）**：`Vec2/3/4`、`Mat4`（列主序，`m[col*4+row]`）、`perspective`、`lookAt`，
加一个 Bresenham 画线，画出一个线框立方体，并打印 8 个顶点的 NDC 坐标手工核对。

## D3 · 2026-11-20 —— 透视校正 + 纹理 + Blinn-Phong

**做了**：

- **Part 1 透视校正**：`SVert` 同时存 `uv` 和 `uv/w`，内层插值 `attr/w` 与 `1/w` 后相除；
  `drawTriangle` 留了 `perspectiveCorrect` 开关，方便出"错误 vs 正确"对照图。
- **Part 2 双线性**：新增 `Texture`（`texel` 内部 clamp、半纹素偏移 `uv*size-0.5`、三次 lerp）。
- **Part 3 光照**：`Vertex` 加 `normal`，新增 `VSOut` / `Fragment` / `IShader`；
  `normalMatrix = transpose(inverse(upper3x3(model)))`；`PhongShader` 实现 ambient + Lambert + 半程向量高光。

**验证**：

```text
透视校正：沿 x=480 的横格线间距 14→19→23→29→39→56（等比，正确）
          仿射      ：13→24→26→26→24→51 →  不规则（错误），差 16.2% 像素
双线性  ：64×64 纹素 / 8×8 格，灰度直方图双峰 70956 : 71046，中间 201 级灰
法线矩阵：纯旋转时 normalMatrix 与 upper3x3 误差 1.5e-8
          非等比缩放 + 旋转时 n'·t' = 0（逆转置） vs 1.875（直接用 M）
高光    ：--no-spec 与默认图差 2982 像素，全在球体上
```

**卡点**：

- 立方体是平面朗伯体，`shininess=48` 时整个面要么全亮要么全不亮，高光根本"看不见"。
  加了一个 UV 球（法线 = 归一化位置）当高光演示物 —— 光滑曲面上才有一片亮斑。
- 法线没归一化的差异只在球体上（2227 像素），立方体完全没变化：因为每个面的三个顶点法线相同，
  插值出来长度就是 1。这条"坑"必须用曲面模型才演示得出来。

**下一步（D4）**：近平面裁剪（齐次空间 Sutherland–Hodgman）+ OBJ 加载。
