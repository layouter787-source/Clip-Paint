package com.clippaint

import android.content.Context
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView

/** Superficie do canvas: a renderizacao (OpenGL ES 3) e feita no motor nativo; aqui so repassamos os toques. */
class CanvasView(context: Context) : SurfaceView(context), SurfaceHolder.Callback {
    var onTouchStart: (() -> Unit)? = null

    init {
        holder.addCallback(this)
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        Native.surfaceCreated(holder.surface)
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        Native.requestRender()
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        Native.surfaceDestroyed()
    }

    override fun onTouchEvent(ev: MotionEvent): Boolean {
        val action = ev.actionMasked
        if (action == MotionEvent.ACTION_DOWN) {
            parent?.requestDisallowInterceptTouchEvent(true)
            onTouchStart?.invoke()
        }
        val n = ev.pointerCount
        val hist = if (action == MotionEvent.ACTION_MOVE) ev.historySize else 0
        val data = DoubleArray(4 + n * (6 + hist * 4))
        data[0] = action.toDouble()
        data[1] = ev.actionIndex.toDouble()
        data[2] = n.toDouble()
        data[3] = ev.eventTime / 1000.0
        var pos = 4
        for (i in 0 until n) {
            data[pos] = ev.getPointerId(i).toDouble()
            data[pos + 1] = ev.getToolType(i).toDouble()
            data[pos + 2] = ev.getX(i).toDouble()
            data[pos + 3] = ev.getY(i).toDouble()
            data[pos + 4] = ev.getPressure(i).toDouble()
            data[pos + 5] = hist.toDouble()
            pos += 6
            for (h in 0 until hist) {
                data[pos] = ev.getHistoricalX(i, h).toDouble()
                data[pos + 1] = ev.getHistoricalY(i, h).toDouble()
                data[pos + 2] = ev.getHistoricalPressure(i, h).toDouble()
                data[pos + 3] = ev.getHistoricalEventTime(h) / 1000.0
                pos += 4
            }
        }
        Native.touch(data)
        return true
    }
}
