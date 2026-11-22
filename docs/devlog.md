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

## D1 · 2026-11-04 —— 数学库与线框

**做了**：

- `math/vec.h`：`Vec2/3/4`（聚合类型，能直接 `{x,y,z}` 初始化）、`dot` / `cross` / `length` / `normalize`
- `math/mat.h`：`Mat4` 列主序 `m[col*4+row]`、`identity`、`operator()(r,c)`、矩阵乘矩阵 / 矩阵乘向量、
  `perspective` / `lookAt` / `translation` / `scaling` / `rotationX` / `rotationY`
- `pipeline/rasterizer.*`：`SVert`、`toScreen`（NDC → 屏幕，y 翻转）、`drawLine`（Bresenham）
- `main.cpp`：立方体 12 条边线框 + 打印 8 个顶点 NDC

**验证**：

```text
(1,1,1) → ndc(0.244, 0.433, 0.952) → screen(597, 153)   手算与程序一致
out.tga 线框 1712 像素，包围盒 x[363..597] y[153..387]，与 8 个顶点的屏幕坐标吻合
```

**卡点**：

- 手算 NDC 时把 z 算成了 -0.301，实际是 0.952。原因：透视除法之后的 z 不是线性的，
  按"深度 / 距离"的比例去推会算错——必须老实算 `clip.z / clip.w`。
- 头文件里直接写函数定义，漏了 `inline`，链接期报重复定义（header-only 的必修课）。

## D2 · 2026-11-04 —— 边函数光栅化 + 深度 + 剔除

**做了**：

- `edge(ax,ay,bx,by,px,py) = (px-ax)(by-ay) - (py-ay)(bx-ax)`：2D 叉积的 z 分量，正负就是"点在边的哪一侧"
- `drawTriangle`：算面积 → 退化三角形直接丢 → 背面剔除（`area < 0`）→ 包围盒 → 像素中心 `(x+0.5, y+0.5)`
  重心坐标 → 深度 `z01 = z*0.5+0.5` → 深度测试 → 写 depth + color
- 立方体 12 个三角形全部 CCW 朝外，每个面独立 4 顶点（法线不能跨面共享）

**验证**：

```text
关剔除（12 个面全画、靠深度）与开剔除：输出逐像素一致
  绿(90,190,110) 29436 px / 蓝(60,90,200) 25044 px / 灰(120,120,130) 4485 px，中心像素为绿
```

**卡点**：

- 屏幕 y 向下，绕序会反转：NDC 里逆时针的三角形，到屏幕上成了顺时针。正面判据得重新推一次符号。
- `w0 = (v1.x, v1.y, ...)` 少写了 `edge(`，逗号表达式把整个调用变成"取最后一个值"，
  编译器只给一句 `left operand of comma operator has no effect`，运行时表现为整片三角形乱涂。

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

## D4 · 2026-11-22 —— OBJ 加载 + 场景 + 轨道相机 + 实时窗口

**做了**：

- 拆出 `scene/`：`mesh.h`（`Mesh{verts, indices}` + 程序化立方体/网格地面/UV 球）、
  `primitives.cpp`、`obj_loader.*`、`camera.h`（`OrbitCamera`）
- **OBJ 解析**：只认 `v/vt/vn/f`；`f` 支持 `v`、`v/vt`、`v//vn`、`v/vt/vn` 四种写法，
  索引可为负（相对文件末尾）；多边形扇形三角化；顶点按 `(v,vt,vn)` 三元组去重；
  没有 `vn` 的文件用面法线补成硬边
- **轨道相机**：`yaw/pitch/distance` 球面参数，`eye() = target + dir*distance`；
  pitch 卡在 ±1.5 rad，避免到 90° 时 `lookAt` 的 up 与视线共线
- **输入**：`FrameInput` 加 `mouseDX/mouseDY/wheel`（增量而非绝对坐标），
  左键拖动旋转、滚轮缩放、R 复位、S 存图
- **演示场景**：网格地面 + 圆环（OBJ，贴图 + 强高光）+ 蓝色球（纯色，高光最清楚）
- `tools/gen_obj.py` 生成 `assets/torus.obj`（56×26 段，2912 三角形）

**验证**：

```text
加载：assets/torus.obj：1456 顶点，2912 三角形
边界：四边形 + 负索引 + 无 vn 的 OBJ → 6 顶点 2 三角形（扇形三角化 + 面法线补全）
      v//vn 形式 → 3 顶点 1 三角形
      文件不存在 → 提示后回退到程序化立方体，不崩
性能：--bench 60 → 12.22 ms/帧（82 FPS），6496 三角形，960×540，无窗口
      窗口模式连续跑 3 秒无异常退出
```

**卡点**：

- 地面原本是一整块 12×12 的四边形，镜头压低后**远端角点跑到相机背后**，`clip.w <= 0` 触发
  "整片丢弃"，地面直接消失。切成 32×32 网格后，被丢的只剩看不见的边角——
  这是"没有近平面裁剪"的临时对策，D5 用 Sutherland–Hodgman 真正解决。
- 贴图原来只有 clamp，没有 wrap。地面 uv 超出 [0,1] 后整片采样到边缘纹素，
  变成一片惨白：**看起来像光照过曝，其实是采样**。加了 `Wrap::Repeat` 才对。
- 圆环绕序写反了，背面剔除把外表面全剔掉，只剩内表面——表现为"模型是黑的"，
  而不是"模型不见了"。生成器里把三角形顺序换成 `(a,c,b)` 就正了。

**下一步（D5）**：齐次空间近平面裁剪（Sutherland–Hodgman，一个平面就够）+ 单元测试骨架。
