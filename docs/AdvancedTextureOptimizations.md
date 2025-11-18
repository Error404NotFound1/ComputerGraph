# 高级纹理贴图升级说明

本轮改动面向 **旗杆 / 导弹弹身、地面、旗面**，在尽量复用现有贴图的前提下，实现三类高级纹理效果，并提供可配置的开关。以下文档描述实现思路、关键代码点以及开关位置，方便日后维护或扩展。

---

## 总体结构
| 材质对象 | 技术要点 | 相关开关（`AppConfig`） |
| --- | --- | --- |
| 旗杆 + 导弹弹身 | 程序化拉丝 bump + 环境映射 | `enableFlagpoleMetalMaterial`、`enableMissileMetalMaterial` |
| 地面砖块/土地 | Triplanar + 程序噪声法线 | `enableGroundProceduralMapping` |
| 旗面布料 | 各向异性高光 + 微法线扰动 + 轻度环境反射 | `enableFlagClothAnisotropy` |

所有开关默认开启，可在 `include/core/AppConfig.h` 中切换。App 启动时会将配置传入 `SceneRenderer`，从而决定是否使用这些高级材质模式。

---

## 1. 旗杆与导弹弹身：拉丝金属 + 环境映射

### 实现要点
1. **材质判定**：`SceneRenderer` 在 draw 之前为每个 `GpuMesh` 计算 `uMaterialMode`（1=金属）。旗杆 pole/ball、导弹 mesh 在开关开启的情况下归入同一模式。
2. **程序化拉丝 bump**：在 `standard.frag` 中的 `brushedMetalNormal()` 使用世界坐标的极坐标角度生成 `sin(θ * 80)` 高频扰动，沿圆周方向 perturb 法线，模拟拉丝纹路，无需额外 normal map。
3. **环境映射**：
   - `SkyboxRenderer` 新增 `dayTextureHandle()` / `nightTextureHandle()`，App 初始化后将 cubemap 纹理传给 `SceneRenderer`。
   - Shader 内通过 `uEnvironmentDay/Night` + `uEnvironmentBlend` 采样天空盒，并按 fresnel+金属性混合至最终颜色。

### 效果
旗杆与导弹表面出现细腻环纹，实时反射天空色彩，光泽度远高于此前的静态贴图。

---

## 2. 地面：Triplanar + 程序噪声法线

### 实现要点
1. **Triplanar Diffuse**：`sampleGroundTriplanar()` 对世界坐标在三个平面上采样（复用已有 diffuse）。权重由法线绝对值决定，避免 UV 拉伸。
2. **程序化 bump**：`groundBumpNormal()` 基于 `proceduralHeight(worldPos)` 的多重正弦噪声计算梯度，扰动法线以形成砖缝凹凸。
3. **开关**：`enableGroundProceduralMapping` 控制该模式是否启用。

### 效果
地表不再受限于单一 UV；近景可见凹凸层次，远景也保持平滑过渡，提升整体真实感。

---

## 3. 旗面：各向异性布料高光 + 微法线

### 实现要点
1. **微法线**：`flagMicroNormal()` 使用 UV 高频正弦和世界高度扰动旗面法线，使织物拥有轻微起伏。
2. **各向异性高光**：在计算 specular 时额外基于 `dFdx(fs_in.worldPos)` 估算切线，形成沿纤维方向拉伸的高光。布面在光照变化时会出现丝绸般的亮带。
3. **环境反射（弱强度）**：旗面模式下使用低 metalness 的环境反射，增加柔和光泽。

### 效果
旗帜在飘动过程中呈现丝绸质感，高光沿布纹滑动，同时保持旗面本来的颜色。

---

## Shader 接口改动摘要
- `standard.frag`：
  - 新增 uniforms：`uEnvironmentDay/Night`, `uEnvironmentBlend`, `uHasEnvironmentMap`, `uMaterialMode`.
  - 新增工具函数：`brushedMetalNormal`, `groundBumpNormal`, `flagMicroNormal`, `sampleGroundTriplanar`, `sampleEnvironment`.
  - 按 `uMaterialMode` 分支，处理不同法线/高光/颜色逻辑。
- `SceneRenderer`：
  - 新增 `MaterialFeatureToggles`、`setAdvancedMaterialToggles()`、`setEnvironmentMaps()`。
  - Draw 时绑定 cubemap，并设置 `uMaterialMode`、`uHasEnvironmentMap` 等 uniform。

---

## 配置与使用说明
1. 在 `AppConfig` 中可切换四个开关：  
   ```cpp
   bool enableFlagpoleMetalMaterial{true};
   bool enableMissileMetalMaterial{true};
   bool enableGroundProceduralMapping{true};
   bool enableFlagClothAnisotropy{true};
   ```
2. 构建运行后可立即看到对应效果。若需要回退至旧材质，只需将开关置 `false`，无需修改 shader。
3. 环境映射依赖天空盒；若关闭天空盒或使用普通 2D 纹理，需保持 `uHasEnvironmentMap=0`，Shader 会自动忽略环境贡献。

---

## 后续扩展建议
- 若将来需要更真实的 PBR，可在相同框架下增加 `roughness/metalness` 贴图输入。
- 地面 triplanar 函数允许传入不同贴图（例如 detail map），可进一步提升贴图分辨率。
- 布料材质可叠加动画噪声或使用屏幕空间法线，打造更复杂的纹理变化。

---

以上即本次高级纹理贴图优化的文档说明。若需在其它模型复用这些效果，只要在 `SceneRenderer::determineMaterialMode` 中添加名称匹配或材质标记，即可迅速扩展。

