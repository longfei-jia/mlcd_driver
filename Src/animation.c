//
// Created by longf on 2025/12/27.
//

#include "animation.h"
#include <stdlib.h> // for rand, abs
#include <stdio.h> // for sprintf

#define BOX_COUNT 5

typedef struct {
    int x, y;       // 位置
    int dx, dy;     // 速度
    int size;       // 边长
} Box;

static Box boxes[BOX_COUNT];

/**
 * @brief 检测两个方块是否相交 (AABB 碰撞检测)
 */
static int CheckCollision(Box *a, Box *b) {
    return (a->x < b->x + b->size &&
            a->x + a->size > b->x &&
            a->y < b->y + b->size &&
            a->y + a->size > b->y);
}

/**
 * @brief 初始化动画状态
 */
void Animation_Init(void) {
    srand(1234); // 固定种子

    for (int i = 0; i < BOX_COUNT; i++) {
        // 尝试生成不重叠的方块
        int valid_pos = 0;
        int attempts = 0;
        
        while (!valid_pos && attempts < 100) {
            boxes[i].size = 10 + (rand() % 20); // 大小 10-30 随机
            boxes[i].x = rand() % (MLCD_WIDTH - boxes[i].size);
            boxes[i].y = rand() % (MLCD_HEIGHT - boxes[i].size);
            
            valid_pos = 1;
            // 检查是否与已生成的方块重叠
            for (int j = 0; j < i; j++) {
                if (CheckCollision(&boxes[i], &boxes[j])) {
                    valid_pos = 0;
                    break;
                }
            }
            attempts++;
        }

        // 速度 -1 到 1，排除 0 (减慢速度)
        do { boxes[i].dx = (rand() % 3) - 1; } while(boxes[i].dx == 0);
        do { boxes[i].dy = (rand() % 3) - 1; } while(boxes[i].dy == 0);
    }
}

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ... (existing code for 2D animation)

// ----------------------------------------------------------------------------
// 3D Cube Animation Implementation
// ----------------------------------------------------------------------------

typedef struct {
    float x, y, z;
} Point3D;

typedef struct {
    int x, y;
} Point2D;

// 正方体顶点 (中心在原点, 边长为 2)
static Point3D cube_vertices[8] = {
    {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
    {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1}
};

// 正方体边 (连接顶点的索引)
static int cube_edges[12][2] = {
    {0,1}, {1,2}, {2,3}, {3,0}, // Back face
    {4,5}, {5,6}, {6,7}, {7,4}, // Front face
    {0,4}, {1,5}, {2,6}, {3,7}  // Connecting edges
};

static float angle_x = 0;
static float angle_y = 0;
static float angle_z = 0;

/**
 * @brief 3D 投影和旋转通用辅助函数
 */
static void ProjectAndDraw(Point3D *vertices, int v_count, int (*edges)[2], int e_count, float scale) {
    Point2D projected_points[32]; // 假设最大顶点数不超过 32
    if (v_count > 32) v_count = 32;

    int offset_x = MLCD_WIDTH / 2;
    int offset_y = MLCD_HEIGHT / 2;

    // 旋转并投影所有顶点
    for (int i = 0; i < v_count; i++) {
        float x = vertices[i].x;
        float y = vertices[i].y;
        float z = vertices[i].z;

        // 绕 X 轴旋转
        float temp_y = y * cosf(angle_x) - z * sinf(angle_x);
        float temp_z = y * sinf(angle_x) + z * cosf(angle_x);
        y = temp_y;
        z = temp_z;

        // 绕 Y 轴旋转
        float temp_x = x * cosf(angle_y) + z * sinf(angle_y);
        temp_z = -x * sinf(angle_y) + z * cosf(angle_y);
        x = temp_x;
        z = temp_z;

        // 绕 Z 轴旋转
        temp_x = x * cosf(angle_z) - y * sinf(angle_z);
        temp_y = x * sinf(angle_z) + y * cosf(angle_z);
        x = temp_x;
        y = temp_y;

        // 简单的正交投影
        projected_points[i].x = (int)(x * scale) + offset_x;
        projected_points[i].y = (int)(y * scale) + offset_y;
    }

    // 绘制所有边
    for (int i = 0; i < e_count; i++) {
        int p1_idx = edges[i][0];
        int p2_idx = edges[i][1];
        
        MLCD_DrawLine(projected_points[p1_idx].x, projected_points[p1_idx].y,
                      projected_points[p2_idx].x, projected_points[p2_idx].y,
                      MLCD_COLOR_BLACK);
    }
}

static void ShowFPS(void) {
    static uint32_t last_tick = 0;
    static int frame_count = 0;
    static int fps = 0;
    
    frame_count++;
    uint32_t current_tick = HAL_GetTick();
    if (current_tick - last_tick >= 1000) {
        fps = frame_count;
        frame_count = 0;
        last_tick = current_tick;
    }
    
    char fps_str[16];
    sprintf(fps_str, "FPS: %d", fps);
    MLCD_DrawString(2, 2, fps_str, MLCD_COLOR_BLACK);
}

// ----------------------------------------------------------------------------
// 1. Cube Animation
// ----------------------------------------------------------------------------

void Animation3D_Cube_Init(void) {
    angle_x = 0; angle_y = 0; angle_z = 0;
}

void Animation3D_Cube_Run(void) {
    MLCD_ClearBuffer();
    angle_x += 0.03f; angle_y += 0.05f; angle_z += 0.02f;
    ProjectAndDraw(cube_vertices, 8, cube_edges, 12, 30.0f);
    ShowFPS();
    MLCD_Refresh();
}

// ----------------------------------------------------------------------------
// 2. Pyramid (Tetrahedron) Animation
// ----------------------------------------------------------------------------

static Point3D pyramid_vertices[4] = {
    {0, -1, 0},      // Top point
    {-1, 1, -1},     // Base 1
    {1, 1, -1},      // Base 2
    {0, 1, 1}        // Base 3 (approx triangle base)
};

static int pyramid_edges[6][2] = {
    {0,1}, {0,2}, {0,3}, // Sides
    {1,2}, {2,3}, {3,1}  // Base
};

void Animation3D_Pyramid_Init(void) {
    angle_x = 0; angle_y = 0; angle_z = 0;
    // 调整一下顶点使其更像正四面体
    float h = 1.0f / sqrtf(2.0f);
    pyramid_vertices[0] = (Point3D){0, -1, 0};
    pyramid_vertices[1] = (Point3D){-1, 1, -h};
    pyramid_vertices[2] = (Point3D){1, 1, -h};
    pyramid_vertices[3] = (Point3D){0, 1, h};
}

void Animation3D_Pyramid_Run(void) {
    MLCD_ClearBuffer();
    angle_x += 0.04f; angle_y -= 0.03f;
    ProjectAndDraw(pyramid_vertices, 4, pyramid_edges, 6, 40.0f);
    ShowFPS();
    MLCD_Refresh();
}

// ----------------------------------------------------------------------------
// 3. Sphere (Wireframe) Animation
// ----------------------------------------------------------------------------

#define SPHERE_RINGS 8
#define SPHERE_SEGS 12
#define SPHERE_V_COUNT (SPHERE_RINGS * SPHERE_SEGS + 2) // +2 for poles
#define SPHERE_E_COUNT (SPHERE_RINGS * SPHERE_SEGS * 2 + SPHERE_SEGS * 2)

static Point3D sphere_vertices[SPHERE_V_COUNT];
static int sphere_edges[SPHERE_E_COUNT][2];
static int sphere_initialized = 0;

void Animation3D_Sphere_Init(void) {
    angle_x = 0; angle_y = 0; angle_z = 0;
    
    if (sphere_initialized) return;
    
    int v_idx = 0;
    
    // Generate vertices
    // Top pole
    sphere_vertices[v_idx++] = (Point3D){0, -1, 0};
    
    for (int r = 0; r < SPHERE_RINGS; r++) {
        float phi = M_PI * (r + 1) / (SPHERE_RINGS + 1);
        for (int s = 0; s < SPHERE_SEGS; s++) {
            float theta = 2.0f * M_PI * s / SPHERE_SEGS;
            float x = sinf(phi) * cosf(theta);
            float y = -cosf(phi); // Y up/down
            float z = sinf(phi) * sinf(theta);
            sphere_vertices[v_idx++] = (Point3D){x, y, z};
        }
    }
    
    // Bottom pole
    sphere_vertices[v_idx++] = (Point3D){0, 1, 0};
    
    // Generate edges
    int e_idx = 0;
    
    // Top cap
    for (int s = 0; s < SPHERE_SEGS; s++) {
        sphere_edges[e_idx][0] = 0;
        sphere_edges[e_idx][1] = 1 + s;
        e_idx++;
    }
    
    // Body
    for (int r = 0; r < SPHERE_RINGS - 1; r++) {
        for (int s = 0; s < SPHERE_SEGS; s++) {
            int cur = 1 + r * SPHERE_SEGS + s;
            int next = 1 + r * SPHERE_SEGS + (s + 1) % SPHERE_SEGS;
            int below = 1 + (r + 1) * SPHERE_SEGS + s;
            
            sphere_edges[e_idx][0] = cur;
            sphere_edges[e_idx][1] = next; // Horizontal
            e_idx++;
            
            sphere_edges[e_idx][0] = cur;
            sphere_edges[e_idx][1] = below; // Vertical
            e_idx++;
        }
    }
    
    // Bottom ring horizontal
    for (int s = 0; s < SPHERE_SEGS; s++) {
        int cur = 1 + (SPHERE_RINGS - 1) * SPHERE_SEGS + s;
        int next = 1 + (SPHERE_RINGS - 1) * SPHERE_SEGS + (s + 1) % SPHERE_SEGS;
        sphere_edges[e_idx][0] = cur;
        sphere_edges[e_idx][1] = next;
        e_idx++;
    }

    // Bottom cap
    int bottom_pole_idx = v_idx - 1;
    for (int s = 0; s < SPHERE_SEGS; s++) {
        int cur = 1 + (SPHERE_RINGS - 1) * SPHERE_SEGS + s;
        sphere_edges[e_idx][0] = cur;
        sphere_edges[e_idx][1] = bottom_pole_idx;
        e_idx++;
    }
    
    sphere_initialized = 1;
}

void Animation3D_Sphere_Run(void) {
    MLCD_ClearBuffer();
    angle_x += 0.02f; angle_y += 0.04f;
    // Sphere has many vertices, use custom ProjectAndDraw to avoid stack overflow
    // Re-implementing simplified version for Sphere due to array size limits in helper
    
    //Point2D projected_point;
    int offset_x = MLCD_WIDTH / 2;
    int offset_y = MLCD_HEIGHT / 2;
    float scale = 45.0f;
    
    // We need to store projected points to draw edges
    // Static buffer to avoid stack overflow
    static Point2D sphere_proj[SPHERE_V_COUNT];

    for (int i = 0; i < SPHERE_V_COUNT; i++) {
        float x = sphere_vertices[i].x;
        float y = sphere_vertices[i].y;
        float z = sphere_vertices[i].z;

        // Rotate
        float temp_y = y * cosf(angle_x) - z * sinf(angle_x);
        float temp_z = y * sinf(angle_x) + z * cosf(angle_x);
        y = temp_y; z = temp_z;

        float temp_x = x * cosf(angle_y) + z * sinf(angle_y);
        temp_z = -x * sinf(angle_y) + z * cosf(angle_y);
        x = temp_x; z = temp_z;

        sphere_proj[i].x = (int)(x * scale) + offset_x;
        sphere_proj[i].y = (int)(y * scale) + offset_y;
    }

    for (int i = 0; i < SPHERE_E_COUNT; i++) {
        int p1 = sphere_edges[i][0];
        int p2 = sphere_edges[i][1];
        MLCD_DrawLine(sphere_proj[p1].x, sphere_proj[p1].y,
                      sphere_proj[p2].x, sphere_proj[p2].y,
                      MLCD_COLOR_BLACK);
    }

    ShowFPS();
    MLCD_Refresh();
}

/**
 * @brief 运行一帧动画
 */
void Animation_Run(void) {
    static uint32_t last_tick = 0;
    static int frame_count = 0;
    static int fps = 0;
    
    // 1. 清除显存
    MLCD_ClearBuffer();

    // ... (existing box update logic)
    
    // 2. 更新位置和处理碰撞
    for (int i = 0; i < BOX_COUNT; i++) {
        // 更新位置
        boxes[i].x += boxes[i].dx;
        boxes[i].y += boxes[i].dy;

        // 2.1 边界碰撞检测
        if (boxes[i].x <= 0) {
            boxes[i].x = 0;
            boxes[i].dx = -boxes[i].dx;
        } else if (boxes[i].x + boxes[i].size >= MLCD_WIDTH - 1) {
            boxes[i].x = MLCD_WIDTH - 1 - boxes[i].size;
            boxes[i].dx = -boxes[i].dx;
        }

        if (boxes[i].y <= 0) {
            boxes[i].y = 0;
            boxes[i].dy = -boxes[i].dy;
        } else if (boxes[i].y + boxes[i].size >= MLCD_HEIGHT - 1) {
            boxes[i].y = MLCD_HEIGHT - 1 - boxes[i].size;
            boxes[i].dy = -boxes[i].dy;
        }
    }

    // 2.2 方块间碰撞检测 (简单的弹性碰撞)
    for (int i = 0; i < BOX_COUNT; i++) {
        for (int j = i + 1; j < BOX_COUNT; j++) {
            if (CheckCollision(&boxes[i], &boxes[j])) {
                // 简单的动量交换模拟：交换速度
                int temp_dx = boxes[i].dx;
                int temp_dy = boxes[i].dy;
                boxes[i].dx = boxes[j].dx;
                boxes[i].dy = boxes[j].dy;
                boxes[j].dx = temp_dx;
                boxes[j].dy = temp_dy;

                // 简单的分离逻辑
                if (boxes[i].x < boxes[j].x) boxes[i].x -= 2; else boxes[i].x += 2;
                if (boxes[i].y < boxes[j].y) boxes[i].y -= 2; else boxes[i].y += 2;
            }
        }
    }

    // 3. 绘制所有方块
    for (int i = 0; i < BOX_COUNT; i++) {
        for (int k = 0; k < boxes[i].size; k++) {
            MLCD_DrawLine(boxes[i].x, boxes[i].y + k, 
                          boxes[i].x + boxes[i].size, boxes[i].y + k, 
                          MLCD_COLOR_BLACK);
        }
    }

    // 4. 绘制外框
    MLCD_DrawLine(0, 0, 127, 0, MLCD_COLOR_BLACK);
    MLCD_DrawLine(127, 0, 127, 127, MLCD_COLOR_BLACK);
    MLCD_DrawLine(127, 127, 0, 127, MLCD_COLOR_BLACK);
    MLCD_DrawLine(0, 127, 0, 0, MLCD_COLOR_BLACK);
    
    // 5. 计算并显示 FPS
    frame_count++;
    uint32_t current_tick = HAL_GetTick();
    if (current_tick - last_tick >= 1000) {
        fps = frame_count;
        frame_count = 0;
        last_tick = current_tick;
    }
    
    char fps_str[16];
    sprintf(fps_str, "FPS: %d", fps);
    MLCD_DrawString(2, 2, fps_str, MLCD_COLOR_BLACK);
    
    // 6. 刷新显存
    MLCD_Refresh();
}
