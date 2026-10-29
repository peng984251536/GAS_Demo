# 《林间小世界》截帧解析：4 个核心 Pass

Oct 19, 2026 · @Penny

## 导读

《林间小世界》的画面不靠描边和色阶，靠的是四件事：用法线“骗”出来的柔软树冠、完全程序化的草叶几何、软件追踪的动态 GI，以及 Tony McMapface 显示变换。本文逐一拆这 4 个 Pass，每个都给出图形学原理、从 SPIR-V 还原的 HLSL 和 RenderDoc 中间缓冲截图。

截帧：`小时间1.rdc`（蓝紫黄昏）与 `小时间2.rdc`（暖阳午后），Vulkan，内部缓冲 1920×1088，输出 1920×1080。下文的 EID 除特别说明外都指第二帧。

**整帧顺序**（加粗为本文展开的 Pass）：

1. 程序化准备（EID 805 起）：草分块、前缀和、植被与屋瓦生成、间接绘制列表
2. 三级级联阴影：1200×1200×3 的 D32
3. **G-buffer**（EID 4041）：`R32G32_UINT`，含 **树冠 4060**、**草 4688 / 4695**
4. Forward：天空与少量对象
5. **GI**：SSGI 4872 → 追踪候选 4903 → 更新 / 求值 / 重投影 / 滤波 4910–4933
6. 屏幕阴影 4941 → **分类延迟光照**：草 4954、**树冠 4961**、实体 4968、植物 4982
7. 透明（云）、体积、景深、TAA
8. **最终色调映射**（EID 5133），然后是 UI

**如何阅读**：每节按「截帧证据 → 图形学原理 → HLSL 还原 → UE 对应」组织。HLSL 是从 RenderDoc 的 SPIR-V 反汇编整理的（原始源码未随包分发），变量名尽量沿用反汇编里的名字，常量来自实际读取的常量缓冲。标「推断」的部分是我根据数据流补全的，不是逐条指令证据。

## Pass 1 树冠：用团块法线和推深度做出“软体积”

树冠看起来像一团柔软的体积，靠三件事：叶卡是面向相机的公告板，用团块法线而不是卡片法线打光；背面的卡在顶点阶段就被剔除；像素阶段再按贴图 G 通道把深度往后推最多 1.6 米，卡片交叉处就不再有硬切线。

**截帧证据**：G-buffer 绘制 EID 4060（`vkCmdDrawIndexedIndirectCount`），光照 EID 4961。顶点输入有 `prim_center`、`prim_normals`、`Vertex_Color`（xy 是卡片角点）；像素阶段采样 256×256 的 `canopy_alpha_texture`，并声明了 `DepthReplacing`，也就是会改写深度。

&#91;image: 第二帧右上角树冠局部：叶簇轮廓清楚，内部却是连续的体积明暗\]

### 1.1 顶点阶段：法线引导的公告板

**原理**。一张叶卡如果用自己的平面法线打光，整棵树会变成一堆明暗乱跳的碎片。这里每张卡携带所属团块在该处的法线（`prim_normals`），光照只看它，等于给一个椭球体打光，再用叶片形状去裁边。这是「法线传递 / Normal Proxy」的实时版。

卡片角点在**视图空间**展开，所以卡永远正对相机；顺带绕「相机→簇中心」轴做一个小的随机旋转，避免排列感。团块背面的卡（法线背离相机）把位置写成 NaN，整张丢弃；轮廓附近的卡用 smoothstep 缩小，轮廓因此是一圈逐渐变小的叶簇，而不是大卡片的硬边。

```hlsl
// EID 4060 VS，由 SPIR-V 整理；省略生长动画与树干弯曲
float3 C  = InstanceTransform(prim_center);          // 叶簇中心（世界）
float3 Nb = InstanceRotate(prim_normals);           // 团块法线
float  facing = dot(Nb, normalize(C - CameraPos));
if (facing >= 0.3) { SV_Position = NaN; return; }  // 背面整张剔除
float s = smoothstep(0.3, -0.2, facing);
float shrink = 1 - (1 - s) * (1 - s);               // 轮廓处缩到 0
shrink *= lerp(0.8, 1.0, saturate(-viewZ * 0.05));  // 近处略缩小

// 角点在视图空间展开 => 公告板；风让卡在屏幕平面内摆动
float2 corner = Vertex_Color.xy * 2 - 1;
float  a = 0.2 * sin(t * 3.53 * wind + px) * sin(t * 1.54 * wind + pz);
corner = mul(corner, float2x2(cos(a), sin(a), -sin(a), cos(a)));
float3 P = swayedPos + mul(float3(corner, 0), (float3x3)ViewToWorld) * cardSize;

// 绕“相机→簇中心”轴随机旋转，再按 shrink 缩放
P = C + mul(P - C, AxisAngle(normalize(CameraPos - C), randAngle)) * shrink;
```

顶点阶段还把整张卡的裁剪空间 z 按 1.6 的偏移重算过一次，作为像素阶段改写深度的基准。

### 1.2 像素阶段：三通道贴图与深度塑形

这张贴图不是颜色贴图，叶色完全由参数决定。拆开三个通道看得很清楚：R 是裁剪形状，G 是每片叶子的前后高度，B 是每片叶子一个不同的灰度编号。

&#91;image: canopy\_alpha 的三个通道分离\]

**原理一：深度塑形（Depth Sprite / Pixel Depth Offset）**。平面卡片彼此穿插时，交线是一条直线，立刻暴露「这是片」。把每个像素按 G 往后推不同距离，卡片就像有了高度起伏，交线变成沿叶片边缘的自然遮挡。截帧用反向 Z，NDC 深度与 1/线性深度成正比，所以推深度只需一次除法。

**原理二：视线锥镂空**。树离相机 15 米以外、又落在相机前方 19°（乘 `cone_angle_mult`）的锥内时，用蓝噪声抖动丢弃像素。这是为了让树冠不挡住玩家正在编辑的画面中心。

```hlsl
// EID 4060 PS，由 SPIR-V 整理；省略花朵分支与雪
float4 m = CanopyAlpha.Sample(LinearClamp, uv);
if (m.r < 0.2) discard;                              // R：叶簇轮廓

float linearZ = 1.0 / SV_Position.w;                 // 视空间深度（米）
float pushed  = linearZ + (1.0 - m.g) * 1.6;          // G 越小推得越远
SV_Depth = SV_Position.z * linearZ / pushed;          // 反向 Z：z_ndc ∝ 1/z

// 每个实例一个稳定随机数，在两种叶色间插值
float  r = Hash01(instanceId * 1128782);              // 移位异或哈希，取 23 位尾数
float3 albedo = lerp(canopy_color_1.rgb, canopy_color_2.rgb, r);

// R32G32_UINT G-buffer：sqrt 是廉价的 gamma，给暗部更多 8 位精度
gbuf.x = PackUnorm4x8(float4(Nb * 0.5 + 0.5, saturate(m.b - 1.0 / 255)));
gbuf.y = PackUnorm4x8(float4(sqrt(saturate(albedo)), m.g));
```

| G-buffer | RGB（3×8 位） | A（8 位） |
| --- | --- | --- |
| x | 团块法线 ×0.5+0.5 | B 通道：叶片编号 |
| y | sqrt(albedo) | G 通道：厚度 / 深度 |

### 1.3 光照阶段（EID 4961）：包裹漫反射 + 宽瓣光泽 + 推入式阴影

这是树冠专用的延迟光照 Pass，只处理树冠像素。五个关键点：

1. **推入式阴影查询**：重建世界坐标后，沿视线再往里推 G×1.6 米去采阴影。树冠内部因此得到渐变的自阴影，而不是只有表面一层。
2. **软阴影**：6 次 Vogel 盘（黄金角）采样，每次用 exp(−1024·Δz) 做指数比较，而不是 0/1 比较。
3. **包裹漫反射**：权重在 0.16–0.8 之间平滑变化，背光面不会死黑。
4. **宽瓣光泽 / 透射**：用按叶片编号扰动过的法线算一个 α²=0.5 的 GGX 分布，乘边缘权重；逆光时不再受 N·L 限制，整个轮廓发亮。这就是叶片「粒粒分明」的亮点来源。
5. **间接光**：半分辨率 L1 球谐，深度感知上采样后提饱和 17%，再用 ZH3 从 L1 推出一个二阶带补方向性（链路见 Pass 3）。

```hlsl
// EID 4961 PS（直接光），由 SPIR-V 整理
float3 N  = GBufNormal;                         // 团块法线
uint   id = GBufX_A;                            // B 通道编号（0–255）
float3 Nj = normalize(N + (id > 0 ? 0.5 * RandomUnitVector(Hash(id)) : 0));
float  G  = GBufY_A;
float3 V  = -viewRay;                           // 指向相机

float sh = SoftShadow6Tap(P + viewRay * G * 1.6);           // 推入树冠再查阴影
float wrap = 0.8 * lerp(0.2, 1.0, smoothstep(-0.2, 1.0, dot(N, L)));
float3 diffuse = albedo / PI * wrap * SunColor * sh;

float rim  = lerp(0.2, 0.7, pow(1 - abs(dot(N, V)), 4));
float nh   = dot(Nj, normalize(L + V));
float d    = 1 - 0.5 * nh * nh;
float D    = (1 / (2 * PI)) / (PI * d * d);                  // 宽瓣 GGX，α² = 0.5
float back = max(0, -0.75 * dot(V, L));                      // 逆光程度
float thin = lerp(id > 0, saturate(exp(-0.4 * dot(N, V * (1 - 2 * G) * 5))), 0.5);
float fade = lerp(0.8, 0.3, smoothstep(50, 150, length(P.xz)));
float3 sheen = rim * D * lerp(wrap, 1, back) * lerp(thin, 1, back) * SunColor * sh * fade;

float3 direct = diffuse + 1.5 * sheen;
```

```hlsl
// EID 4961 PS（间接光），由 SPIR-V 整理
SH1RGB sh = BilateralUpsample3(IndirectRadiance);   // w = exp2(-min(30, 80*|1 - zHalf/z|))
float3 L0 = max(0, lerp(Luma(sh.L0) * tintDir, sh.L0, 1.17) * 1.2); // 提饱和
float3 E  = 0.8862 * L0 + 1.0233 * float3(dot(sh.L1r, N), dot(sh.L1g, N), dot(sh.L1b, N));
E += 0.25 * ZH3(sh, N);            // ZH3：L2 带 ≈ L0 * (0.08r + 0.6r²)，沿 L1 主方向
float3 indirect = albedo * max(0, E) * 1.3 * lerp(0.3, 1, saturate(exp(-0.2 * thickTerm)));
// 另有反射方向的 SH 求值 × 0.04 的弱镜面间接光，以及最多 32 盏闪烁灯笼点光
```

0.8862 和 1.0233 是 L1 球谐卷积余弦瓣的标准系数（π·Y₀₀ 与 2π/3·Y₁），0.3154 是 Y₂₀ 的归一化系数。ZH3 的 0.08r + 0.6r² 与 Roughton 等人 2024 年论文《ZH3: Quadratic Zonal Harmonics》的公式一致（推断：系数吻合，源码无注释）。

## Pass 2 草地：程序化草叶 + 用地形法线打光

草地的「绒毛毯」质感来自一个反直觉的做法：每根草都是真实几何，但主光照用的是**地形法线**，草叶自己的法线只用来算高光。所以整片草地的明暗跟着地形起伏走，远看是一张平滑的绒面，近看又有一根根的闪光。

**截帧证据**：近景 EID 4688 `grass_shader`，一次间接绘制 39 indices × 37627 instances；远景 EID 4695 `grass_lod1_shader`，411387 vertices × 1 instance。草叶数据由 EID 1017 的计算 Shader 生成（64 线程组），它读取地形高度图以及路径、建筑、水面、花园、岩石、物体推开等 7 张遮罩，然后写入 `GrassBladeDataPacked` 缓冲和间接绘制参数。光照在 EID 4954。

&#91;image: 第二帧右下草地局部：明暗跟随地形起伏，草尖偏黄、草根偏暗\]

### 2.1 数据：一根草 32 字节

| 字段 | 内容 | 用途 |
| --- | --- | --- |
| position | float3 | 草根世界坐标 |
| color | RGBA8（存 sqrt） | 草色，在 VS 里平方还原 |
| packed0 | 4×8 位 | r：叶形参数 s；g：宽度；b：倾斜角；a：朝向 x |
| packed1 | 2×half | 高度、弯曲量 b |
| packed2 | 2×half | 风响应（刚度）、朝向 z |

### 2.2 顶点阶段：一条带子弯成一根草

**原理**。没有顶点缓冲，只用 `SV_VertexID` 和 `SV_InstanceID`：索引最低位决定左右两侧，其余位决定从根到尖的参数 t。形状全是解析函数：宽度剖面、二次弯曲、绕朝向旋转、绕 x 倾斜。三个值得学的细节：

- **俯视时更弯**：弯曲量乘 lerp(1, 2, |视线.y|)。俯视时草会倾倒更多，露出更多叶面，避免从上往下看只剩一片片细线。
- **按屏幕尺寸补偿**：算出该深度下 30 像素对应多少米，超过 1 米就把草按比例放大，同时把草根往下沉，草尖高度不变。远处的草因此不会细到亚像素而闪烁。
- **风**：世界 xz × 0.1 加 time × 1.2 做 2D 值噪声，再过 sin 变成一阵一阵的风浪，按 t² 作用（草根不动），同时压弯法线。

&#91;image: 按 VS 公式绘制的草叶宽度、弯曲与风场（风场的哈希为示意）\]

```hlsl
// EID 4688 VS，由 SPIR-V 整理
GrassBlade g = Unpack(grass_blade_buffer_data[SV_InstanceID]);
float side = (SV_VertexID & 1) ? 1 : -1;
float segs = 2 * exp2((1 - g.s) * 2.3219);                    // 2–10 段
float t    = saturate((SV_VertexID & ~1u) / segs);

// 宽度剖面：s=0 等宽收尖，s=1 上宽梯形（PS 再挖 V 口）
float w = lerp(0.04 * saturate(2.5 * (1 - t * t)), 0.135 * t, g.s);
float h = t * lerp(1.0, 1.1, g.s);

// 屏幕尺寸补偿：k = 该深度下 30 像素对应的米数，至少为 1
float pxPerMeter = 0.5 * Proj[1][1] * min(2048, ResY) / -viewZ;
float k = max(30.0 / pxPerMeter, 1.0);
float3 root = g.pos; root.y += (1 - k) * g.height * 2.5;      // 下沉，草尖高度不变

// 二次弯曲，俯视时加倍
float b = 0.03 * g.bend * lerp(1, 2, abs(normalize(CameraPos - root).y));
float3 local = float3(side * w, h, 3 * b * t * t);

// 曲线导数得法线，再向两侧偏 0.3 => 假圆柱法线
float3 n = normalize(cross(float3(1, 0, 0), float3(0, dh, 6 * b * t)));
n.x += 0.3 * side;

float3 P = root + RotateFacing(RotateLean(local * scale * k, g.lean), g.facing);

// 风
float nz   = ValueNoise2D(P.xz * 0.1 + float2(time * 1.2, 0));
float gust = (sin(nz * 3.14 - 1.57) + 0.3) * 0.7 * t * t * g.windResponse
           * smoothstep(-0.2, 0.5, g.height * 0.2);
P.x -= 0.25 * gust;  n.y -= 0.7 * gust;

float3 color = lerp(g.color * 0.8, g.color, t * t);          // 草根压暗 20%
// 另外用 ImageGather 对高度图 4 点差分，算出草根处的地形法线传给 PS
```

### 2.3 像素阶段：两个法线塑进一个 G-buffer

草的 G-buffer 与树冠同一格式，但内容不同：RGB 存的是**地形法线**，草叶法线用八面体编码压成两个 8 位值，拆开放进两个 A 通道。叶形也在这里定型：按横向坐标 |u| 和 t 裁掉中间一块，s 大的梯形会被挖出 V 形缺口，一条带子看起来就是分叉的两根草。

```hlsl
// EID 4688 PS，由 SPIR-V 整理
float u = abs(C9.x / C9.z);                        // -1..1 横向坐标
if (u < lerp(-0.8 / s, 1.0, t)) discard;          // 从顶端挖 V 口
float3 Nb = dot(V, bladeN) <= 0 ? -bladeN : bladeN; // 双面：草叶法线翻向相机
float3 albedo = color * 0.55 * lerp(0.7, 1.1, t);   // 根暗尖亮
float2 oct = OctEncode(Nb) * 0.5 + 0.5;
gbuf.x = PackUnorm4x8(float4(terrainN * 0.5 + 0.5, oct.x));
gbuf.y = PackUnorm4x8(float4(sqrt(saturate(albedo)), oct.y));
```

### 2.4 光照阶段（EID 4954）：地形漫反射 + 草叶高光 + 逆光绒光

```hlsl
// EID 4954 PS（直接光），由 SPIR-V 整理
float ntl  = max(0, dot(terrainN, L));
float nbl  = max(0, dot(bladeN, L));
float3 sun = SunColor * (1 - ScreenShadow) * smoothstep(0, 0.5, ntl);

float3 diffuse  = sun * ntl * albedo / PI;                    // 地形法线
float3 specular = sun * nbl * GGX_Smith(bladeN, V, L, 0.0915, F0 = 0.04); // 草叶法线，α²≈0.09

// 逆光绒光：地形掠射角的 Fresnel × 对着太阳看的程度
float fr   = pow(1 - saturate(dot(terrainN, V)), 5);
float back = pow(1 - saturate(dot(L, V)), 3);
float3 direct = diffuse + specular + diffuse * fr * back;
// 间接光与树冠相同（SH + ZH3），并乘 saturate(0.1 + SSGI) 作为遮蔽
```

**为什么好看**：漫反射只看地形，整片草的明暗是低频、干净的；高频细节只由草叶高光和根尖色差提供。通常每根草各用各的法线会产生大量噪点和 TAA 闪烁，这里从根上避开了。

## Pass 3 GI 链路：GTAO 可见性 + 软件 BVH 追踪 + 球谐重建

阴影里仍然有蓝色天光和暖色反射，靠的是两条并行的链路：一条叫 SSGI，实际是 GTAO，只输出可见性标量；另一条先屏幕追踪、失败再走软件 BVH，把入射光压成每像素 3 组 L1 球谐，经时空滤波后交给分类光照。全部在半分辨率 960×544 上完成。

&#91;embedded content: GI 数据流 · 2 条链路、一处汇合\]

GTAO 只给出「有多少方向被挡住」，颜色全部来自中排的追踪链；上一帧结果回灌进追踪，所以多次反弹是跨帧累积出来的。

### 3.1 SSGI（EID 4872）：其实是一切片 GTAO

**原理**。GTAO（Jimenez 2016）把半球积分拆成若干个绕视线的切片。每个切片里沿屏幕两侧步进，找到左右地平线角 h₁、h₂，再把法线投影到切片平面得到角 n，对余弦加权可见性做解析积分。截帧里只有 1 个切片、每侧 3 步（方向和步长用时变噪声打散），靠后面的 spatial / upsample / temporal 三个 Pass 去噪。

证据很硬：快速 acos 近似 `(-0.1566x + 1.5708)·sqrt(1-x)` 和积分式 `0.25·(−cos(2h−n) + cos n + 2h·sin n)` 都与 GTAO 论文逐项吻合，输出下限是 0.03。

```latex
\hat{a}(h, n) = \tfrac{1}{4}\left(-\cos(2h - n) + \cos n + 2h\sin n\right),\quad A = \lVert n_{proj} \rVert\,\big(\hat{a}(h_1, n) + \hat{a}(h_2, n)\big)
```

```hlsl
// EID 4872 CS（8×8），由 SPIR-V 整理
float3 V = normalize(-Pview);
float2 dir = float2(cos(phi), -sin(phi));            // phi = (0 + noise.x) * PI，仅 1 个切片
float3 axis = normalize(cross(sliceDir - V * dot(sliceDir, V), V));
float3 nP = N - axis * dot(N, axis);                  // 法线投影到切片
float  cosN = saturate(dot(nP, V) / length(nP));
float  n = sign(dot(sliceDir, nP)) * FastAcos(cosN);

float c1 = cos(n + HALF_PI), c2 = cos(n - HALF_PI);  // 地平线初值
for (int s = 0; s < 3; ++s) {                         // 每侧 3 步，步长按 ((s+ξ)/3)² 分布
    float2 o = round(dir * radiusPx * (pow((s + frac(ξ + 0.618 * s)) / 3, 2) + minStep));
    float3 d1 = PosAt(uv + o) - Pview, d2 = PosAt(uv - o) - Pview;
    float  f1 = saturate(length(d1) * -80 * k + bias), f2 = saturate(length(d2) * -80 * k + bias);
    c1 = max(c1, lerp(cos(n + HALF_PI), dot(normalize(d1), V), f1));  // 距离衰减
    c2 = max(c2, lerp(cos(n - HALF_PI), dot(normalize(d2), V), f2));
}
float h1 = n + max(-FastAcos(c2) - n, -HALF_PI);     // 取角并夹到半球内
float h2 = n + min( FastAcos(c1) - n,  HALF_PI);
float ao = lerp(length(nP), 1, 0.05) * 0.25 *
           ((-cos(2 * h1 - n) + cosN + 2 * h1 * sin(n)) +
            (-cos(2 * h2 - n) + cosN + 2 * h2 * sin(n)));
Output[px] = max(ao, 0.03);
```

&#91;image: GI 输入：半分辨率法线、GTAO 可见性和屏幕阴影\]

### 3.2 追踪候选（EID 4903）：屏幕优先，BVH 兜底

这个 Compute Shader 反汇编长达 178 KB，结构是标准的混合追踪：

1. 从深度、法线重建起点，用时变序列在余弦半球上取一个方向。
2. 先做屏幕空间光线步进。命中且重投影有效时，直接读上一帧的 `taa_history_tex` 作为入射光。历史帧本身已含间接光，所以多次反弹是「跨帧累积」出来的。
3. 屏幕失败则在计算 Shader 里自己遍历 `rt_bl_bvh`（3354 个节点）和 `rt_triangles`（25250 个三角形），维护栈、求交点，这就是软件光线查询。另有地形高度图追踪与 `terrain_lighting_texture` 分支。
4. 命中几何就用三角形的简化颜色、太阳可见性和受光算一个近似辐射；什么都没命中就取天空高 / 低色。
5. 候选辐射按 RGB 最大分量夹到 10，压制萤火虫。

```hlsl
// EID 4903 CS，结构整理（非逐行）
float3 dir = CosineSampleHemisphere(N, Sequence(frame, px));
float3 Li;
if (ScreenTrace(P, dir, hitUV) && ReprojectValid(hitUV))
    Li = TaaHistory[hitUV];                         // 复用上一帧最终颜色
else if (TraceBVH(P, dir, hit))                     // rt_bl_bvh + rt_triangles，软件遍历
    Li = hit.albedo * (SunColor * SunVisible(hit) * saturate(dot(hit.N, L)) + Ambient(hit));
else
    Li = lerp(sky_low_color, sky_high_color, saturate(dir.y));
Li *= min(1, 10 / max3(Li));                        // 压亮点
```

### 3.3 更新、投影到球谐、重投影、滤波（EID 4910–4933）

- **4910**：把候选辐射、方向、深度写入 `restir_*` 纹理并初始化 reservoir 字段。这里没有读历史 reservoir 做重采样，所以**不能**判定启用了完整 ReSTIR GI，只是命名留了接口。
- **4918 投影到 L1 球谐**：收集周围 8 个样本，按深度相似、GTAO 相似和半球条件加权，用 Y₀₀ = 0.2821、Y₁ = 0.4886 投影。
- **4926 重投影**：用运动向量找历史，维护均值 / 方差统计。
- **4933 滤波**：3×3 邻域，按历史长度选采样间隔（类似 À-Trous），权重包含深度、法线和 GTAO 差异。

```hlsl
// EID 4918 CS：把入射光投影到每个颜色通道一组 L1 SH
float4 basis = float4(0.2821, 0.4886 * rayDir);       // Y00, Y1-1, Y10, Y11
for (int i = 0; i < 8; ++i) {
    float w = DepthWeight(i) * AoWeight(i) * (dot(rayDir_i, N) > 0);
    shR += basis_i * Li_i.r * w;  shG += basis_i * Li_i.g * w;  shB += basis_i * Li_i.b * w;
    wSum += w;
}
// 三组 float4 横向排在同一张 2880×544 纹理的三个 960 宽区域
```

下图把滤波后的 SH 纹理拆开看。L0 是间接光的「平均颜色」：树冠偏蓝（天光），圆塔底部和小屋门口偏暖（地面与墙的反射）。L1 以绿色为主，即 +Y 分量大，说明主要间接光来自上方天空。

&#91;image: 滤波后的 SH 间接光：L0 颜色与 L1 方向\]

### 3.4 光照阶段如何用它

分类光照 Pass 用 3 tap 深度感知上采样读回半分辨率 SH（权重 exp2(−min(30, 80·|1−z半/z|))），再做三件事：用 `indirect_light_tint` 把 L0 提饱和 17% 并 ×1.2；按 L1 辐照度公式求值；用 ZH3 沿 L1 主方向补一个二阶带。代码见 Pass 1.3。

提饱和这一步对风格影响很大：物理上多次反弹的间接光会变灰，这里反向把它推向更鲜艳，暗部因此是有颜色的蓝紫而不是灰黑。

## Pass 4 最终色调映射：受光适应 + Tony McMapface + 轻调色

画面亮部「奶油感」、不刺眼也不跑色，主要来自 Tony McMapface：越亮的颜色越平滑地褶向白色，而不是像直接截断那样变成黄、青等饱和色块。它前面有一个按受光比例压暗的适应系数，后面只有很轻的饱和度（1.02）、对比度（1.05）和 40% 强度的调色 LUT。

**截帧证据**：EID 5133 `Final blit / final_post`，采样 48³ 的 `tony_mc_mapface_lut`、`grading_lut`、`blue_noise_texture`，读取 `shadow_mask_mean`（由 EID 4941 统计、EID 5014 平滑）。第二帧实际常量：饱和度 1.02、对比度 1.05、调色强度 0.4、暗角 0、ICC 关闭。

### 4.1 原理

- **受光适应**：不是普通的 HDR 亮度直方图自动曝光。EID 4941 在屏幕中央区域统计「被太阳照到的比例」，EID 5014 用 1−exp(−0.5·Δt) 缓慢插值。受光越多，整体压得越暗，最低到 0.57 倍。画面大片在阴影里时就不会发闷。
- **预塑形 + LUT 坐标**：pow(x, 1.1)·1.6 是一个轻微的对比和曝光提升；x/(x+1) 把 \[0, ∞) 压到 \[0, 1) 作为 LUT 坐标，再加半个体素的偏移对齐格点中心。
- **Tony McMapface**：一个离线烘焙的 3D LUT，输入是线性 HDR，输出是线性显示色。它模拟「亮度越高，感知饱和度越低」，色相基本保持。
- **保色对比**：对比度只作用在亮度上（rgb × lum^c / lum），不会把饱和度顺带推高。
- **三角分布蓝噪声抖动**：把均匀蓝噪声变成 ±1/255 的三角分布，并乘 saturate(color·512)，纯黑不会被抖亮。天空和雾的渐变因此没有色带。

### 4.2 HLSL

```hlsl
// EID 5133 PS，由 SPIR-V 整理
float3 c = Bicubic5Tap(InputTex, uv);                 // 1920×1088 → 1920×1080 锐化重采样
c *= lerp(1.0, 0.57, smoothstep(0.3, 1.0, saturate(shadow_mask_mean[0])));
c *= exp(-2 * vignette_amount * pow(length(uv - 0.5), 3));   // 本帧 vignette = 0

float3 x   = pow(c, 1.1) * 1.6;
float3 uvw = (x / (x + 1)) * (47.0 / 48.0) + 0.5 / 48.0;
float3 m   = TonyLUT.SampleLevel(LinearClamp, uvw, 0).rgb;  // 线性显示色
m = max(m - 0.005, 0) * 1.005;                               // 黑位微调

float L = dot(m, float3(0.2126, 0.7152, 0.0722));
m = saturate(lerp(L, m, grading_saturation));                // 1.02
L = dot(m, float3(0.2126, 0.7152, 0.0722));
m *= pow(L, grading_contrast) / max(L, 0);                    // 1.05，只动亮度

float3 g = SrgbToLinear(GradingLUT.SampleLevel(LinearClamp, LinearToSrgb(m), 0).rgb);
m = lerp(m, g, grading_strength);                             // 0.4

float n = BlueNoise[(px + frame * int2(59, 37)) & 255].x * 2 - 1;
float tri = max(-1, n * rsqrt(abs(n))) - sign(n);            // 均匀 → 三角分布
m = saturate(m + saturate(m * 512) * tri / 255);
```

### 4.3 前后对比

下图用 Tony McMapface 的公开 LUT（即截帧里同名的 48³ 纹理），把上面的链路在一组 HDR 色带上复现，与直接截断和逐通道 Reinhard 对比。未包含受光适应，也未包含内容未导出的 grading LUT。

&#91;image: 同一组 HDR 色带在三种显示变换下的结果与灰阶响应\]

三点观察：

- **截断**在 +2 EV 以上色相跑偏，红变黄、蓝变青，形成硬色块。
- **Reinhard** 不跑色，但高光始终保持饱和，灰阶中段也偏暗偏平。
- **截帧链路**中段与截断几乎重合（中间调保持原样），高光平滑地褶向白色。这就是阳光下草地和墙面「亮但柔和」的原因。

## UE 复现对照与可信度说明

大部分技巧在 UE 里用材质 + WPO + 后处理就能近似；做不到的是自定义 G-buffer 布局和树冠 / 草地专用光照模型，那需要源码版引擎加自定义 Shading Model。

| 截帧技巧 | UE 对应做法 | 还原度 |
| --- | --- | --- |
| 叶卡公告板 + 团块法线 | Blender 生成叶卡，法线传递到椭球团块；团块中心写进 UV2/UV3，WPO 里在视图空间展开角点 | 高 |
| 背面卡剔除、轮廓缩小 | WPO 按 dot(团块法线, 相机方向) 缩放到 0 | 高 |
| G 通道推深度 | Masked 材质的 Pixel Depth Offset = (1−G) × 160 cm | 高 |
| 包裹漫反射 + 宽瓣光泽 | Two Sided Foliage + Subsurface Color 近似；精确公式需自定义 Shading Model | 中 |
| 草叶程序化 + 屏幕尺寸补偿 | Blender 做 3–5 个草叶变体，PCG 或 Grass Tool 散布；WPO 做弯曲、风、远处放大 | 高 |
| 草用地形法线打光 | 草模型法线全部朝上，或在材质里用 RVT 读取地形法线 | 中高 |
| GTAO + BVH 追踪 + SH GI | Lumen（软件光追）+ 屏幕空间 AO；用 Post Process 的 Indirect Lighting Intensity / Color 提饱和 | 中 |
| 受光比例适应 | 固定曝光，用蓝图按阴影占比改 Exposure Compensation | 中 |
| Tony McMapface | 后处理材质放在 Replacing the Tonemapper，导入 48³ LUT 做体纹理采样 | 高 |

**可信度说明**

- HLSL 是从 RenderDoc 显示的 SPIR-V 反汇编整理的（`sourceDebugInformation=false`，没有原始源码）。变量名、函数名是我按数据流起的，常数和运算顺序与反汇编一致。标「结构整理」的代码块省略了分支，不是逐行对应。
- GTAO、ZH3、L1 SH 的判断依据是系数与公式结构吻合，源码里没有注释或命名直接证实。
- `restir_*` 纹理存在，但当前编译版本没有做时空重采样，不能说启用了 ReSTIR GI。
- 「风场示意」「色调对比」两张图是按 Shader 公式重新计算的；前者的哈希函数为示意，后者未含 grading LUT 和受光适应。其他中间缓冲图来自实际回放导出；SH 纹理是浮点数据的 8 位导出，负值被截断，只能看趋势。
- 未采集 GPU 耗时，不对性能下结论。原始导出与脚本在 `D:\ArtResource\RenderDoc\xiaoshijian_analysis`。

**参考**

- Jimenez et al., *Practical Real-Time Strategies for Accurate Indirect Occlusion*（GTAO）, 2016
- Roughton et al., *ZH3: Quadratic Zonal Harmonics*, i3D 2024
- [Tony McMapface（h3r2tic）](https://github.com/h3r2tic/tony-mc-mapface)
- [Peter-Pike Sloan：Stupid Spherical Harmonics Tricks](https://www.ppsloan.org/publications/StupidSH36.pdf)
