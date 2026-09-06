// CaptureController.qml —— 覆盖层交互状态机（全 QML）
//
// 唯一的交互决策者：接收输入（press/move/release/key），
// 调用后端（backend）原语完成 绘制/裁剪/标注移动删除/撤销重做/导出收尾。
import QtQuick
import QtQuick.Controls
import Frap

QtObject {
    id: root

    // ---- 模式 ----
    readonly property int modeIdle:       0
    readonly property int modeDrawing:    1
    readonly property int modeCropDraw:   2
    readonly property int modeCropMove:   3
    readonly property int modeCropResize: 4
    readonly property int modeOpMove:     5

    property int mode: modeIdle

    // ---- 状态 ----
    property string toolId: backend.activeTool
    property bool   cropTool: backend.cropToolActive
    property string selectedOp: ""
    property rect   cropRect: backend.hasCrop ? backend.cropRect : Qt.rect(0, 0, 0, 0)
    property string textBuffer: ""
    property bool   textFocused: false
    property point  textAnchor: Qt.point(0, 0)
    property point  mousePos: Qt.point(-1, -1)
    property string readout: ""
    property int    cursorShape: Qt.ArrowCursor
    property bool   hoveringOp: false
    property rect   selectedBounds: Qt.rect(0, 0, 0, 0)
    property rect   opGhostBounds: Qt.rect(0, 0, 0, 0)

    // ---- 拖拽过程状态 ----
    property point  dragStart: Qt.point(0, 0)
    property point  dragCurrent: Qt.point(0, 0)
    property rect   dragCrop: Qt.rect(0, 0, 0, 0)
    property rect   dragCropBase: Qt.rect(0, 0, 0, 0)
    property int    resizeHandle: -1
    property string opGrabId: ""
    property point  opGrabStart: Qt.point(0, 0)
    property point  opGrabDelta: Qt.point(0, 0)

    // 裁剪区（视觉用：拖拽时用过程值，否则用已提交值）
    readonly property rect effectiveCrop: {
        if (mode === modeCropDraw || mode === modeCropMove || mode === modeCropResize)
            return dragCrop
        return cropRect
    }
    readonly property bool hasEffectiveCrop: effectiveCrop.width > 0 && effectiveCrop.height > 0

    // ---- 手柄几何 ----
    function handlePoint(r, i) {
        switch (i) {
        case 0: return Qt.point(r.x, r.y)
        case 1: return Qt.point(r.x + r.width / 2, r.y)
        case 2: return Qt.point(r.x + r.width, r.y)
        case 3: return Qt.point(r.x + r.width, r.y + r.height / 2)
        case 4: return Qt.point(r.x + r.width, r.y + r.height)
        case 5: return Qt.point(r.x + r.width / 2, r.y + r.height)
        case 6: return Qt.point(r.x, r.y + r.height)
        default:return Qt.point(r.x, r.y + r.height / 2)
        }
    }

    function cropHandleAt(x, y) {
        var r = effectiveCrop
        if (r.width <= 0 || r.height <= 0)
            return -1
        for (var i = 0; i < 8; ++i) {
            var p = handlePoint(r, i)
            if (Math.abs(x - p.x) <= 7 && Math.abs(y - p.y) <= 7)
                return i
        }
        return -1
    }

    function insideCrop(x, y) {
        var r = effectiveCrop
        return x > r.x + 3 && x < r.x + r.width - 3 &&
               y > r.y + 3 && y < r.y + r.height - 3
    }

    function normRect(a, b) {
        var x = Math.min(a.x, b.x), y = Math.min(a.y, b.y)
        return Qt.rect(x, y, Math.abs(b.x - a.x), Math.abs(b.y - a.y))
    }

    function clamp(v, lo, hi) { return Math.max(lo, Math.min(hi, v)) }

    function updateCursor() {
        if (cropTool || toolId.length)
            cursorShape = Qt.CrossCursor
        else if (mode === modeOpMove)
            cursorShape = Qt.ClosedHandCursor
        else if (hoveringOp)
            cursorShape = Qt.PointingHandCursor
        else if (cropHandleAt(mousePos.x, mousePos.y) >= 0 && hasEffectiveCrop)
            cursorShape = Qt.SizeAllCursor
        else
            cursorShape = Qt.ArrowCursor
    }

    function refreshSelected() {
        if (selectedOp.length) {
            var b = backend.opBounds(selectedOp)
            selectedBounds = b ? Qt.rect(b.x, b.y, b.width, b.height) : Qt.rect(0, 0, 0, 0)
        } else {
            selectedBounds = Qt.rect(0, 0, 0, 0)
        }
        opGhostBounds = selectedBounds
    }

    // ---- 输入 ----
    function press(x, y, button) {
        mousePos = Qt.point(x, y)
        readout = backend.readoutAt(x, y)
        if (button === Qt.RightButton) {
            cancelCurrent()
            return
        }
        if (button !== Qt.LeftButton)
            return
        dragStart = Qt.point(x, y)
        dragCurrent = Qt.point(x, y)

        if (cropTool) {
            if (backend.hasCrop) {
                var h = cropHandleAt(x, y)
                if (h >= 0) { mode = modeCropResize; resizeHandle = h; dragCropBase = backend.cropRect; dragCrop = backend.cropRect; return }
                if (insideCrop(x, y)) { mode = modeCropMove; dragCropBase = backend.cropRect; dragCrop = backend.cropRect; return }
            }
            mode = modeCropDraw
            dragCrop = Qt.rect(0, 0, 0, 0)
            return
        }

        if (toolId.length > 0) {
            var it = backend.interactionOf(toolId)
            if (it === "click-point") {
                textAnchor = Qt.point(x, y)
                if (backend.autoType(toolId) === "number") {
                    backend.applyTool(toolId, {"pointX": x, "pointY": y}, {})
                    return
                }
                textFocused = true
                textBuffer = ""
                return
            }
            mode = modeDrawing
            return
        }

        // 空闲：标注选中 / 裁剪调整
        var op = backend.opIdAt(x, y)
        if (op.length > 0) {
            selectedOp = op
            refreshSelected()
            opGrabId = op
            opGrabStart = Qt.point(x, y)
            opGrabDelta = Qt.point(0, 0)
            mode = modeOpMove
            return
        }
        if (backend.hasCrop) {
            var h2 = cropHandleAt(x, y)
            if (h2 >= 0) { mode = modeCropResize; resizeHandle = h2; dragCropBase = backend.cropRect; dragCrop = backend.cropRect; return }
            if (insideCrop(x, y)) { mode = modeCropMove; dragCropBase = backend.cropRect; dragCrop = backend.cropRect; return }
        }
        selectedOp = ""
    }

    function move(x, y) {
        mousePos = Qt.point(x, y)
        readout = backend.readoutAt(x, y)
        dragCurrent = Qt.point(x, y)

        switch (mode) {
        case modeDrawing:
            backend.setToolPreview(toolId, geometryForDraw(), {})
            break
        case modeCropDraw:
            dragCrop = normRect(dragStart, dragCurrent)
            break
        case modeCropMove:
            dragCrop = Qt.rect(dragCropBase.x + (x - dragStart.x),
                               dragCropBase.y + (y - dragStart.y),
                               dragCropBase.width, dragCropBase.height)
            break
        case modeCropResize:
            dragCrop = resizeCrop(dragCropBase, resizeHandle, x, y)
            break
        case modeOpMove:
            opGrabDelta = Qt.point(x - opGrabStart.x, y - opGrabStart.y)
            break
        case modeIdle:
            var op = backend.opIdAt(x, y)
            hoveringOp = op.length > 0 && !cropTool && toolId.length === 0
            updateCursor()
            break
        }
        if (mode === modeOpMove) {
            opGhostBounds = Qt.rect(selectedBounds.x + opGrabDelta.x,
                                    selectedBounds.y + opGrabDelta.y,
                                    selectedBounds.width, selectedBounds.height)
        }
    }

    function release() {
        switch (mode) {
        case modeDrawing:
            backend.clearPreview()
            var geom = geometryForDraw()
            var small = geom["rectX"] !== undefined
                        && (geom["rectW"] < 3 || geom["rectH"] < 3)
            if (!small)
                backend.applyTool(toolId, geom, {})
            break
        case modeCropDraw:
            if (dragCrop.width < 12 || dragCrop.height < 12)
                backend.clearCrop()
            else
                backend.setCrop(dragCrop.x, dragCrop.y, dragCrop.width, dragCrop.height)
            break
        case modeCropMove:
        case modeCropResize:
            backend.setCrop(dragCrop.x, dragCrop.y, dragCrop.width, dragCrop.height)
            break
        case modeOpMove:
            if (opGrabDelta.x !== 0 || opGrabDelta.y !== 0)
                backend.moveOp(opGrabId, opGrabDelta.x, opGrabDelta.y)
            opGrabId = ""
            break
        }
        mode = modeIdle
    }

    function geometryForDraw() {
        var it = backend.interactionOf(toolId)
        if (it === "drag-line")
            return {"startX": dragStart.x, "startY": dragStart.y,
                    "endX": dragCurrent.x, "endY": dragCurrent.y}
        var r = normRect(dragStart, dragCurrent)
        return {"rectX": r.x, "rectY": r.y, "rectW": r.width, "rectH": r.height}
    }

    function resizeCrop(base, handle, x, y) {
        var r = base
        var minW = 12, minH = 12
        switch (handle) {
        case 0: r.x = Math.min(x, base.x + base.width - minW); r.y = Math.min(y, base.y + base.height - minH); break
        case 1: r.y = Math.min(y, base.y + base.height - minH); break
        case 2: r.y = Math.min(y, base.y + base.height - minH); r.width = Math.max(x - base.x, minW); break
        case 3: r.width = Math.max(x - base.x, minW); break
        case 4: r.width = Math.max(x - base.x, minW); r.height = Math.max(y - base.y, minH); break
        case 5: r.height = Math.max(y - base.y, minH); break
        case 6: r.x = Math.min(x, base.x + base.width - minW); r.height = Math.max(y - base.y, minH); break
        case 7: r.x = Math.min(x, base.x + base.width - minW); break
        }
        return r
    }

    // ---- ESC / 右键：逐级取消 ----
    function cancelCurrent() {
        if (mode !== modeIdle) {
            if (mode === modeDrawing)
                backend.clearPreview()
            mode = modeIdle
            opGrabId = ""
            return
        }
        if (textFocused) {
            textFocused = false
            textBuffer = ""
            return
        }
        if (toolId.length > 0) {
            backend.deactivateTool()
            return
        }
        if (cropTool) {
            if (backend.hasCrop)
                backend.clearCrop()
            backend.deactivateTool()
            return
        }
        backend.cancel()
    }

    // ---- 工具条动作 ----
    function onToolbar(id) {
        if (id === "tool.crop")
            backend.toggleCropTool()
        else if (id === "undo")
            backend.undo()
        else if (id === "redo")
            backend.redo()
        else if (id === "cancel")
            backend.cancel()
        else if (id === "done")
            backend.finish()
        else if (id.indexOf("tool.") === 0)
            backend.setActiveTool(id)
        else if (id.indexOf("export.") === 0)
            backend.exportAction(id)
        else if (id.indexOf("service.") === 0 || id.indexOf("capture.") === 0)
            backend.serviceAction(id)
    }

    // ---- 快捷键 ----
    function onKey(key, text, modifiers) {
        var ctrl = (modifiers & Qt.ControlModifier)
        var shift = (modifiers & Qt.ShiftModifier)
        if (textFocused) {
            if (key === Qt.Key_Escape) { textFocused = false; textBuffer = "" }
            else if (key === Qt.Key_Return || key === Qt.Key_Enter) commitText()
            else if (key === Qt.Key_Backspace) textBuffer = textBuffer.slice(0, -1)
            else if (text.length > 0 && text.charCodeAt(0) >= 32) { textBuffer += text; updateTextPreview() }
            return true
        }
        if (key === Qt.Key_Escape) { cancelCurrent(); return true }
        if (key === Qt.Key_Return || key === Qt.Key_Enter) { backend.finish(); return true }
        if (key === Qt.Key_Delete || key === Qt.Key_Backspace) { if (selectedOp.length) { backend.deleteOp(selectedOp); selectedOp = "" } return true }
        if (ctrl && key === Qt.Key_Z) { backend.undo(); return true }
        if (ctrl && key === Qt.Key_Y || (ctrl && shift && key === Qt.Key_Z)) { backend.redo(); return true }
        if (ctrl && key === Qt.Key_C) { backend.exportAction("export.clipboard"); return true }
        if (ctrl && key === Qt.Key_S) { backend.exportAction("export.save"); return true }
        if (ctrl && key === Qt.Key_D) { backend.finish(); return true }
        var t = text.toLowerCase()
        if (t === "c") { backend.toggleCropTool(); return true }
        if (toolByShortcut(t)) return true
        return false
    }

    function toolByShortcut(s) {
        var tb = backend.toolbar
        for (var i = 0; i < tb.length; ++i) {
            var e = tb[i]
            if (e.type === "tool" && e.shortcut === s) {
                if (e.id === toolId)
                    backend.deactivateTool()
                else
                    backend.setActiveTool(e.id)
                return true
            }
        }
        return false
    }

    // ---- 文本工具 ----
    function updateTextPreview() {
        if (toolId.length && textBuffer.length)
            backend.setToolPreview(toolId,
                {"pointX": textAnchor.x, "pointY": textAnchor.y, "text": textBuffer}, {})
        else
            backend.clearPreview()
    }
    function commitText() {
        var text = textBuffer.trim()
        textFocused = false
        textBuffer = ""
        backend.clearPreview()
        if (text.length)
            backend.applyTool(toolId,
                {"pointX": textAnchor.x, "pointY": textAnchor.y, "text": text}, {})
    }
    function clearText() {
        textBuffer = ""
        textFocused = false
        backend.clearPreview()
    }

    // 绑定后端状态变化
    onToolIdChanged: { selectedOp = ""; refreshSelected(); updateCursor(); if (!textFocused) textBuffer = "" }
    onCropToolChanged: { selectedOp = ""; refreshSelected(); updateCursor() }
    onCropRectChanged: if (mode === modeIdle) dragCrop = cropRect
    onSelectedOpChanged: refreshSelected()
}
