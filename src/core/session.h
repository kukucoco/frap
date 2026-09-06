/*
 * session.h —— 截图会话：原图 + 裁剪区 + 非破坏性编辑栈
 *
 * 非破坏性模型：工具插件不直接修改像素，而是返回"操作描述"（JSON op）。
 * 主程序维护 原图 + 操作列表，render() 时按序重放。
 *
 * 撤销/重做：操作列表快照栈。增/移/删/替换 都是"提交一个新列表"，
 * 因此 undo/redo 统一实现，天然支持"移动标注""删除标注"的撤销。
 *
 * 坐标约定：
 *   - 操作(op) 与裁剪区(crop) 使用图像物理像素坐标
 *   - 覆盖层鼠标使用逻辑坐标，经 scale 换算
 *   - 输出 = 裁剪区（若有）裁出的部分，否则全图
 */
#ifndef FRAP_CORE_SESSION_H
#define FRAP_CORE_SESSION_H

#include <QObject>
#include <QImage>
#include <QRect>
#include <QSizeF>
#include <QList>
#include <QJsonObject>

namespace sc {

class CaptureSession : public QObject
{
    Q_OBJECT
public:
    explicit CaptureSession(QObject* parent = nullptr) : QObject(parent) {}

    void setImage(const QImage& image);
    QImage image() const { return m_image; }

    /* 逻辑屏幕尺寸（覆盖层所在输出），用于 逻辑坐标 <-> 物理像素 换算 */
    void setScreenSize(const QSizeF& size) { m_screenSize = size; }
    QSizeF screenSize() const { return m_screenSize; }
    QPointF scale() const; /* 像素/逻辑 */

    /* 逻辑坐标 -> 像素坐标 */
    QPoint logicalToPixel(const QPoint& p) const;
    QRect logicalToPixel(const QRect& r) const;
    QPoint pixelToLogical(const QPoint& p) const;

    /* ---- 裁剪区（决定输出范围；不在撤销栈内） ---- */
    void setCrop(const QRect& pixelRect);
    void clearCrop();
    bool hasCrop() const { return m_hasCrop; }
    QRect cropPixel() const { return m_cropPixel; }
    QRect cropLogical() const;

    /* ---- 编辑栈 ---- */
    void addOperation(const QJsonObject& op);
    /* 移动某操作（dx/dy 为图像像素）；失败返回 false */
    bool moveOperation(const QString& id, int dx, int dy);
    bool deleteOperation(const QString& id);
    QString selectedOpIdAt(const QPoint& pixelPos) const; /* 命中测试，返回 op id 或空 */
    QJsonObject operationById(const QString& id) const;
    int countOperationType(const QString& type) const;

    void undo();
    void redo();
    bool canUndo() const { return !m_undoStack.isEmpty(); }
    bool canRedo() const { return !m_redoStack.isEmpty(); }
    void clearOperations();
    QList<QJsonObject> operations() const { return m_ops; }
    int operationCount() const { return m_ops.size(); }

    /* 渲染结果（原图 + 全部操作） */
    QImage render() const;
    /* 输出图像：裁剪区（若有）裁出的部分，否则全图 */
    QImage renderedOutput() const;
    /* 将输出导出为临时 PNG 文件，返回路径 */
    QString exportToTempPng() const;

signals:
    void imageChanged();
    void operationsChanged();

private:
    void pushState();

    QImage m_image;
    QSizeF m_screenSize;
    bool m_hasCrop = false;
    QRect m_cropPixel;

    QList<QJsonObject> m_ops;
    QList<QList<QJsonObject>> m_undoStack;
    QList<QList<QJsonObject>> m_redoStack;
    int m_nextOpId = 1;
};

} // namespace sc

#endif /* FRAP_CORE_SESSION_H */
