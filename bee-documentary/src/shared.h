// shared.h — C++ 宿主与 Metal 内核共用的常量（只有 #define，两边都能编）。
// 统一参数块 U[] 的下标（每项一个 float4）
#define U_RES        0   // w, h, 1/w, 1/h
#define U_FRAME      1   // frameSeed, sampleBase, spd(每次调度的采样数), layerMask
#define U_CAM_POS0   2
#define U_CAM_POS1   3
#define U_CAM_R0     4
#define U_CAM_R1     5
#define U_CAM_U0     6
#define U_CAM_U1     7
#define U_CAM_F0     8
#define U_CAM_F1     9
#define U_LENS       10  // focus0, focus1, fieldW, apR
#define U_LENS2      11  // catD, catM, rimK, bladeRot
#define U_SUN_DIR    12  // xyz, cos(半角)
#define U_SUN_E      13  // rgb 辐照度
#define U_SKY_ZEN    14  // 天顶色
#define U_SKY_HOR    15  // 地平线色
#define U_GROUND     16  // 下半球（植被/地面）色
#define U_COUNTS     17  // tlasRoot(未用), numInst, numVeinFore, numVeinHind
#define U_MISC       18  // time(秒), shutter, debugMode, hairBoost
#define U_FILL       19  // 机位侧的补光方向 xyz, 强度
#define U_FAR0       20  // 远景：模糊半径(弧度), 亮度, seed, 高光数
#define U_FAR1       21  // 远景色调 A
#define U_FAR2       22  // 远景色调 B
#define U_FAR3       23  // 远景色调 C（花色）
#define U_WIND       24  // 远景漂移 xyz, 闪烁相位
#define U_LOWRES     25  // 低分辨率层 w,h (resolve 用)
#define U_FOCUSPT    26  // 备用
#define U_N          32

// 图元类型
#define PT_TRI 0
#define PT_CYL 1
#define PT_SPH 2
// 材质类型
#define M_DIFF  0
#define M_TRANS 1
#define M_FILM  2
#define M_HAIR  3
#define M_SSS   4
#define M_EMIT  5
// 程序纹理
#define T_NONE 0
#define T_PETAL 1
#define T_FLORET 2
#define T_EYE 3
#define T_ABDOMEN 4
#define T_WING 5
#define T_CUTICLE 6
#define T_LEAF 7
#define T_LEG 8
#define T_POLLENBALL 9
#define T_STEM 10
#define T_HEAD 11
#define T_ANTENNA 12
#define T_TONGUE 13
#define T_CORBICULA 14
// 图层（实例 flags 的低位）
#define L_FG   1
#define L_MID  2
#define L_NEAR 4
