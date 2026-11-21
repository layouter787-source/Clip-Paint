package com.clippaint

import android.content.Context
import android.graphics.Color
import android.view.Gravity
import android.view.View
import android.widget.FrameLayout
import android.widget.HorizontalScrollView
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView

/** Timeline de cels; cada faixa corresponde a uma camada raster do documento. */
class AnimationPanel(
    context: Context,
    private val onChanged: () -> Unit
) : LinearLayout(context) {
    private val controls = LinearLayout(context)
    private val tracksHost = LinearLayout(context)
    private val frameInfo = label(context, "", 12f, T.DIM, true)
    private var lastState = IntArray(0)

    private fun actionChip(text: String, description: String, action: () -> Unit): Chip {
        return Chip(context, text, true).apply {
            contentDescription = description
            tooltipText = description
            setOnClickListener { action(); refresh(true); onChanged() }
            layoutParams = lp(WRAP, dp(34f))
        }
    }

    init {
        orientation = VERTICAL
        background = roundBg(T.BAR, dp(22f).toFloat())
        elevation = dp(10f).toFloat()
        setPadding(dp(10f), dp(8f), dp(10f), dp(8f))

        controls.orientation = HORIZONTAL
        controls.gravity = Gravity.CENTER_VERTICAL
        controls.addView(actionChip("‹", "Frame anterior") {
            val s = Native.animationState()
            Native.animationSeek((s.getOrNull(1) ?: 0) - 1)
        })
        controls.addView(actionChip("▶", "Reproduzir ou pausar") {
            val s = Native.animationState()
            Native.animationSetPlaying(s.getOrNull(3) != 1)
        })
        controls.addView(actionChip("›", "Próximo frame") {
            val s = Native.animationState()
            Native.animationSeek((s.getOrNull(1) ?: 0) + 1)
        })
        controls.addView(actionChip("+", "Novo frame vazio") {
            Native.animationInsertFrame(false)
        })
        controls.addView(actionChip("⧉", "Duplicar frame atual") {
            Native.animationInsertFrame(true)
        })
        controls.addView(actionChip("−", "Apagar frame atual") {
            Native.animationRemoveFrame()
        })
        controls.addView(actionChip("FPS−", "Diminuir velocidade") {
            val fps = Native.animationState().getOrNull(0) ?: 24
            Native.animationSetFps((fps - 1).coerceAtLeast(1))
        })
        controls.addView(actionChip("FPS+", "Aumentar velocidade") {
            val fps = Native.animationState().getOrNull(0) ?: 24
            Native.animationSetFps((fps + 1).coerceAtMost(120))
        })
        val controlsScroll = HorizontalScrollView(context).apply {
            isHorizontalScrollBarEnabled = false
            addView(controls, FrameLayout.LayoutParams(WRAP, WRAP))
        }
        addView(controlsScroll, lp(MATCH, WRAP))

        frameInfo.setPadding(dp(4f), dp(6f), dp(4f), dp(6f))
        addView(frameInfo, lp(MATCH, WRAP))

        val scroll = ScrollView(context).apply {
            isFillViewport = false
            isVerticalScrollBarEnabled = false
        }
        tracksHost.orientation = VERTICAL
        scroll.addView(tracksHost, FrameLayout.LayoutParams(MATCH, WRAP))
        addView(scroll, lp(MATCH, dp(164f)))
        refresh(true)
    }

    fun refresh(force: Boolean = false) {
        val state = Native.animationState()
        if (state.size < 8) return
        if (!force && state.contentEquals(lastState)) return
        val previousFrame = lastState.getOrNull(1) ?: -1
        val previousTracks = lastState.getOrNull(4) ?: -1
        val previousCount = lastState.getOrNull(2) ?: -1
        val previousFps = lastState.getOrNull(0) ?: -1
        lastState = state

        val fps = state[0]
        val frame = state[1]
        val count = state[2]
        val playing = state[3] == 1
        frameInfo.text = "Frame ${frame + 1} / $count    •    ${fps} FPS" +
            if (playing) "    •    Reproduzindo" else ""
        (controls.getChildAt(1) as? TextView)?.text = if (playing) "Ⅱ" else "▶"

        if (force || previousFrame != frame || previousTracks != state[4] ||
            previousCount != count || previousFps != fps) {
            rebuildTracks(frame, count)
        }
    }

    private fun rebuildTracks(currentFrame: Int, frameCount: Int) {
        tracksHost.removeAllViews()
        val tracks = Native.animationTrackCount()
        for (trackIndex in 0 until tracks) {
            val line = LinearLayout(context).apply {
                orientation = HORIZONTAL
                gravity = Gravity.CENTER_VERTICAL
            }
            val name = label(context, Native.animationTrackName(trackIndex), 11f, T.TEXT, true)
            name.maxLines = 1
            name.setPadding(dp(4f), 0, dp(8f), 0)
            line.addView(name, lp(dp(78f), dp(36f)))

            val cells = LinearLayout(context).apply {
                orientation = HORIZONTAL
                gravity = Gravity.CENTER_VERTICAL
            }
            val start = (currentFrame - 9).coerceAtLeast(0)
            val end = (start + 24).coerceAtMost(frameCount)
            for (frame in start until end) {
                val cell = TextView(context).apply {
                    text = (frame + 1).toString()
                    gravity = Gravity.CENTER
                    textSize = 11f
                    setTextColor(if (frame == currentFrame) Color.WHITE else T.DIM)
                    background = roundBg(
                        if (frame == currentFrame) T.ACCENT else T.SURFACE3,
                        dp(6f).toFloat()
                    )
                    setOnClickListener {
                        Native.animationSeek(frame)
                        onChanged()
                        refresh(true)
                    }
                    layoutParams = lp(dp(34f), dp(31f)).apply {
                        setMargins(dp(2f), dp(2f), dp(2f), dp(2f))
                    }
                }
                cells.addView(cell)
            }
            val strip = HorizontalScrollView(context).apply {
                isHorizontalScrollBarEnabled = false
                addView(cells, FrameLayout.LayoutParams(WRAP, WRAP))
            }
            line.addView(strip, lp(0, dp(38f)).apply { weight = 1f })
            tracksHost.addView(line, lp(MATCH, dp(40f)))
        }
        if (tracks == 0) {
            tracksHost.addView(label(context, "Crie uma camada para começar a animar.", 12f, T.DIM))
        }
    }
}
