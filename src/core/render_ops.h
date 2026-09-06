/*
 * render_ops.h —— 声明式编辑操作的本地渲染器
 *
 * 工具插件返回的操作是"描述"，而不是像素。主程序（core）内置对这些
 * 操作类型的渲染实现，从而：
 *   - 无需把大图在插件间来回传递（性能）
 *   - 操作可撤销、可重放、可序列化为脚本（步骤记录）
 *   - 插件实现极简：只需输出几何数据
 *
 * 内置操作类型（v1）：
 *   rect      {rect:{x,y,w,h}, color, width, fill?}
 *   ellipse   {rect:{...}, color, width, fill?}
 *   line      {start:{x,y}, end:{x,y}, color, width}
 *   arrow     {start:{x,y}, end:{x,y}, color, width, headSize?}
 *   pen       {points:[{x,y},...], color, width}
 *   text      {point:{x,y}, text, color, fontFamily?, size?, bold?}
 *   number    {point:{x,y}, text, color, size?, textColor?}
 *   highlight {rect:{...}, color}
 *   mosaic    {rect:{...}, cell?, strength?}
 *   blur      {rect:{...}, radius?}
 *
 * 坐标全部为物理像素。
 */
#ifndef FRAP_CORE_RENDER_OPS_H
#define FRAP_CORE_RENDER_OPS_H

#include <QImage>
#include <QList>
#include <QJsonObject>
#include <QString>
#include <QRect>

namespace sc {

QStringList supportedOperationTypes();

/* 依序重放操作，返回渲染结果 */
QImage applyOperations(const QImage& source, const QList<QJsonObject>& ops);

/* 单个操作是否被内置渲染器支持 */
bool isOperationSupported(const QJsonObject& op);

/*
 * 计算操作的包围盒（物理像素坐标，含少量内边距）。
 * 供标注"点选/命中测试"使用；未知类型返回空矩形。
 */
QRect operationBounds(const QJsonObject& op);

/* 点是否命中某操作（矩形：命中包围盒；线/箭头：命中线段附近） */
bool operationHitTest(const QJsonObject& op, const QPoint& pos, int tolerance = 10);

/* 平移操作几何（rect/start/end/point/points），返回新操作 */
QJsonObject translateOperation(const QJsonObject& op, int dx, int dy);

} // namespace sc

#endif /* FRAP_CORE_RENDER_OPS_H */
