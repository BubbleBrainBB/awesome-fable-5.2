// shared.h — 主机与着色器共用的 uniform 结构（全部 float4，避免对齐差异）
#ifndef SHARED_H
#define SHARED_H
#ifdef __METAL_VERSION__
typedef float4 f4;
#else
#include <simd/simd.h>
typedef simd_float4 f4;
#endif

#define TH_N 8192        // 高分辨率高度图边长
#define TT_N 4096        // 树格边长
#define TREE_CELL (24000.0f/4096.0f)
#define MAXRIP 12
#define MAXBIRD 24

struct Cam { f4 pos; f4 right; f4 up; f4 fwd; };   // pos.w: tan(半垂直视场)
struct Uni {
    Cam cam0, cam1;      // 快门开/关时刻的相机
    f4 sunDir;           // xyz 指向太阳
    f4 sunI;             // 大气外太阳辐照（rgb）
    f4 tp;               // x 时间(s) y 帧号 z 采样序号 w 总采样数
    f4 res;              // x 宽 y 高 z 曝光 w 快门时长(s)
    f4 lens;             // x 对焦距离 y 光圈半径(m) z 暗角 w 淡出(0..1 乘子)
    f4 flags;            // x 质量档(0 预演 1 正式) y 是否画翅膀 z 是否有主角树 w 调试视图
    f4 anim0, anim1;     // 快门开/关时刻：x 拍翅相位(周) y 翅展开度 z 栖枝下沉(m) w 刹车量
    f4 hero;             // 主角树（栖枝）基点 xyz, w 朝向角
    f4 perch;            // 栖点 xyz
    f4 rip[MAXRIP];      // 水面涟漪：xz 位置, z=起始时间, w=强度
    f4 bird[MAXBIRD];    // 同伴/飞鸟：xyz 位置, w=拍翅相位；负 w 表示无
    f4 birdv[MAXBIRD];   // 飞鸟速度方向 xyz, w=大小(翼展 m)
    f4 wind;             // xy 风向, z 风速, w 预留
    f4 dbg;              // 后期/调试参数
    f4 tube[8];          // 穿云通道折线（xyz），w=半径；w<=0 表示无
};
#endif
#define TREE_JIT 0.56f   // 树在格内的随机偏移幅度（格宽的比例）
