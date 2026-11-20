package com.clippaint

import android.app.Activity
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import android.os.Bundle
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView

class MainActivity : Activity() {
    private lateinit var drawing: DrawingSurface

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.statusBarColor = Color.rgb(245, 247, 250)
        window.navigationBarColor = Color.rgb(245, 247, 250)
        window.decorView.systemUiVisibility = View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR or
            View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR

        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(Color.rgb(245, 247, 250))
            setPadding(16, 12, 16, 12)
        }
        val header = TextView(this).apply {
            text = "Clip Paint"
            textSize = 22f
            setTextColor(Color.rgb(28, 39, 56))
            gravity = Gravity.CENTER_VERTICAL
            setPadding(4, 8, 4, 16)
        }
        root.addView(header)
        drawing = DrawingSurface()
        root.addView(drawing, LinearLayout.LayoutParams(-1, 0, 1f))

        val tools = LinearLayout(this).apply {
            gravity = Gravity.CENTER
            orientation = LinearLayout.HORIZONTAL
            setPadding(0, 12, 0, 4)
        }
        fun tool(label: String, action: () -> Unit) {
            tools.addView(Button(this).apply {
                text = label
                isAllCaps = false
                setOnClickListener { action() }
            }, LinearLayout.LayoutParams(0, -2, 1f))
        }
        tool("Pincel") { drawing.setEraser(false) }
        tool("Borracha") { drawing.setEraser(true) }
        tool("Desfazer") { drawing.undo() }
        tool("Limpar") { drawing.clear() }
        root.addView(tools)
        setContentView(root)
    }
}

private class DrawingSurface : View(MainActivity@null) {
    private val strokes = mutableListOf<Pair<Path, Paint>>()
    private var activePath: Path? = null
    private var eraser = false
    private val backgroundPaint = Paint().apply { color = Color.WHITE }
    private val strokePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.rgb(35, 92, 180)
        style = Paint.Style.STROKE
        strokeWidth = 7f
        strokeCap = Paint.Cap.ROUND
        strokeJoin = Paint.Join.ROUND
    }

    constructor(context: android.content.Context) : this() { }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        canvas.drawColor(Color.WHITE)
        strokes.forEach { (path, paint) -> canvas.drawPath(path, paint) }
        activePath?.let { canvas.drawPath(it, currentPaint()) }
    }

    private fun currentPaint() = Paint(strokePaint).apply {
        color = if (eraser) Color.WHITE else Color.rgb(35, 92, 180)
        strokeWidth = if (eraser) 34f else 7f
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                activePath = Path().apply { moveTo(event.x, event.y) }
                invalidate()
                return true
            }
            MotionEvent.ACTION_MOVE -> {
                activePath?.lineTo(event.x, event.y)
                invalidate()
                return true
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                activePath?.let { strokes.add(it to currentPaint()) }
                activePath = null
                invalidate()
                performClick()
                return true
            }
        }
        return true
    }

    override fun performClick(): Boolean {
        super.performClick()
        return true
    }

    fun setEraser(enabled: Boolean) { eraser = enabled }
    fun undo() { if (strokes.isNotEmpty()) strokes.removeAt(strokes.lastIndex); invalidate() }
    fun clear() { strokes.clear(); activePath = null; invalidate() }
}
