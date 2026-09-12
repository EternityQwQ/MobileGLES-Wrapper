# MobileGLES-Wrapper 重写说明

分支：`rewrite/dsa-multidraw-gles32`（基于 `68b2630`）
提交：`9bd8a07` 重写 + `88afe0b` 链接修复

## 结论

| 项目 | 状态 |
|---|---|
| `direct_state_access`（GLES 3.2 模拟实现） | 完成 |
| `multidraw`（GLES 3.2 实现，原理不变） | 完成 |
| Android arm64-v8a CI | **通过**（10/10 步骤） |
| 导出符号表面 | 与原版**完全一致** |

CI 运行：<https://github.com/EternityQwQ/MobileGLES-Wrapper/actions/runs/34704975684>
产物：`MobileGlues-Android-arm64-v8a`（2.0 MB，strip 后 aarch64 `.so`）

---

## 一、direct_state_access

**原理不变**：GLES 没有 DSA，所以访问未绑定对象时「临时绑定 → 调用经典入口点 → 还原先前绑定」。

### 结构改动

- 五套几乎逐字重复的按对象族的绑定栈，合并为一个泛型 `PushTempBinding` / `PopTempBinding`，
  底层是单个 `thread_local ankerl::unordered_dense::map<GLenum, std::vector<GLuint>>`；
  配一个 `BindTargetNow(GLenum, GLuint)` switch 和行号唯一的 `TempBind` RAII 宏组。
- `dsa::QueryForTarget` 用扁平 `switch` 取代原来的排序表 + 二分查找。
- `dsa::CurrentBinding` **改为 CPU 优先**：从 gl/ 栈自己的跟踪器
  （`find_bound_buffer`、`find_bound_array`、`mgGetTexObjectByTarget`、
  `current_read_fbo`/`current_draw_fbo`）回答，而不是 `glGetIntegerv`。
  只有 sampler、program pipeline、transform feedback、renderbuffer 这四族
  （legacy 包装层直接转发给 GLES 而不记录）仍需真实查询，每处均注明为冷路径。
- 四个 `glGetQueryBufferObject*` 合并为一个 `QueryResultToBuffer<T>` 模板。

### 修复的既有缺陷

| 位置 | 问题 |
|---|---|
| `glCreateQueries` | 守卫写反（`if (n <= 0 \|\| !ids) glGenQueries(...)`），合法调用反而不生成任何对象 |
| `glCreateSamplers` | 做了一次无意义的 `glGetIntegerv`，并围绕**硬编码的 unit 1** 绑定/还原 |
| `glCreateProgramPipelines` | 同上，无意义的绑定/还原 |
| `glGetVertexArrayIndexed64iv` | 把 `GLint64*` 强转成 `GLint*` 写入，8 字节槽只写 4 字节，高半部分为垃圾 |
| 多处校验分支 | `LOG_W` 之后被注释掉的 `// return;` 恢复为真正的 `return;`（快速失败） |
| `glBindTextureUnit(unit, 0)` | 改为解绑所有可绑定目标，而不是把 `GL_INVALID_ENUM` 当 target 传下去 |

---

## 二、multidraw

**原理不变**：同一套后端阶梯（`Unroll` / `BaseVertex` / `Indirect` / `MultiArrays` /
`MultiIndirect` / `MultiBaseVertex` / `Compute` / `DrawElements`）、同一条
经 `global_settings.multidraw_order` 的顺序驱动降级、同一套 `md_probe_state_t`
探测并锁存、同一套基于 `g_owner_ctx_id` 的上下文失效、同一个四 SSBO 索引融合
着色器（含二级前缀和）、同一个用于 `*IndirectCount` 的紧凑化着色器。

### 结构改动

- 18 个分散的 scratch 全局量收拢进一个 `md_scratch_state_t`。
- 新增 `md_scratch_buffer_t`，把「只增不减的缓冲」与其**经查询验证过**的容量绑在一起——
  失败的分配不再能伪装成大缓冲。
- 四处 `static GLuint prev...` + 手写还原，改为
  `md_indirect_binding_scope_t` / `md_element_binding_scope_t` / `md_ssbo_binding_scope_t<N>`。
- `prepare_indirect_buffer` 按命令布局拆分为二；两个 MultiBaseVertex 入口点共用同一实现体。
- 新增 `mg_multi_draw_indirect_available()`，让两个 indirect 入口点和紧凑化路径共用
  一个能力判定，取代三处逐字重复的条件。
- 两个 `*IndirectCount` 入口共用同一个 `mg_indirect_count`。

### 修复的既有缺陷

- 还原子区间 SSBO 绑定仍走 `glBindBufferRange`（用 `glBindBufferBase` 会把
  子区间静默放大成整个缓冲）；且**提前返回路径上也补上了还原**。
- 恢复了 `mg_glMultiDrawElements_basevertex`：它可经 `glx/lookup.cpp` 的
  按入口点后缀表被 `glXGetProcAddress` 取到，缺失会让该名字解析为 `nullptr`。

---

## 三、验证方式

**关键教训**：`g++ -fsyntax-only` 看不到缺失的函数定义。

第一轮我用 `-fsyntax-only` 只确认了语法，结果 CI 在链接期报：

```
ld.lld: error: undefined symbol: dsa::CurrentBinding(unsigned int)
（41 处引用）
```

于是我下载 Android NDK r27c 并**在本机完整复现 CI 构建流程**，才抓到真正的问题：

```bash
export ANDROID_NDK_HOME=/opt/android-ndk-r27c
cd MobileGlues-cpp
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-21 \
  -DANDROID_STL=c++_static -DSTATICLIB=OFF -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS="-Wno-error=implicit-function-declaration"
cmake --build build --config Release --target mobileglues
```

补上 `dsa::CurrentBinding` 定义后 `BUILD EXIT=0`。

### 符号表面零回归

编译新旧两个 `.o`，比对 `nm --defined-only --extern-only` 输出：

- 原版导出 **47** 个符号，新版 **48** 个；
- **原版有而新版缺失：0**；
- 多出的 1 个是 `md_scratch_buffer_t::ensure` 内的 `MD_WARN_ONCE` 静态锁存变量，文件内局部符号。

### 最终产物

```
build/libmobileglues.so    53 MB → strip 后 6.0 MB
file: ELF 64-bit LSB shared object, ARM aarch64, stripped
DSA + multidraw 相关导出符号: 172 个
```

---

## 四、尚未验证的部分

- **运行期行为未测**：本环境无 Android 设备/模拟器，只验证到「编译链接通过、
  符号正确导出」。着色器在真机驱动上的实际编译、compute 路径的实际执行、
  probe-and-latch 的真实触发，都需要在设备上跑。
- **未改动 `multidraw.h`**：API 表面保持不变，因此无需改动（已用 `git diff` 确认）。
- **`gl/impl/*` 未涉及**：该目录不在 `CMakeLists.txt` 中，且依赖
  `GLStateManager::GetCurrentBufferForTarget`（全树无声明），属于死的未构建代码。

## 五、后续建议

1. 在真机上验证 compute 路径与 `*IndirectCount` 紧凑化路径。
2. 本分支尚未开 PR；如需合并，建议先跑一轮真机回归。
3. 若要把两个 live context 交替使用的情形做干净（当前每次切换都会重建 scratch 对象
   并泄漏旧对象），需要引入按上下文的映射表——当前实现未做，已在代码注释中标注。
