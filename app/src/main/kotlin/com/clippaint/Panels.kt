package com.clippaint

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.ComposeShader
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.PorterDuff
import android.graphics.RectF
import android.graphics.Shader
import android.os.Handler
import android.os.Looper
import android.text.TextUtils
import android.util.LruCache
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.widget.BaseAdapter
import android.widget.FrameLayout
import android.widget.HorizontalScrollView
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.ListView
import android.widget.ScrollView
import android.widget.TextView
import java.util.concurrent.Executors

// ====================================================================== previews

/** Gera (em segundo plano) a pre-visualizacao de cada pincel renderizando um traco real no motor nativo. */
object Previews {
    private const val W = 256
    private const val H = 64
    private val cache = LruCache<Int, Bitmap>(120)
    private val pending = HashSet<Int>()
    private val exec = Executors.newSingleThreadExecutor()
    private val main = Handler(Looper.getMainLooper())
    var listener: (() -> Unit)? = null
    private var notifyQueued = false

    fun get(index: Int): Bitmap? {
        val c = cache.get(index)
        if (c != null) return c
        if (pending.add(index)) {
            exec.execute {
                val px = Native.brushPreview(index, W, H)
                if (px != null) {
                    val bmp = Bitmap.createBitmap(W, H, Bitmap.Config.ARGB_8888)
                    bmp.setPixels(px, 0, W, 0, 0, W, H)
                    cache.put(index, bmp)
                }
                main.post {
                    pending.remove(index)
                    if (!notifyQueued) {
                        notifyQueued = true
                        main.postDelayed({
                            notifyQueued = false
                            listener?.invoke()
                        }, 120)
                    }
                }
            }
        }
        return null
    }
}

// ====================================================================== seletor de cor

class ColorPickerView(context: Context) : View(context) {
    var onColor: ((Int, Boolean) -> Unit)? = null
    private var hue = 0f
    private var sat = 0f
    private var value = 1f
    private var mode = 0  // 1 = quadrado SV, 2 = barra de matiz
    private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val sv = RectF()
    private val bar = RectF()
    private val hueColors = intArrayOf(
        Color.RED, Color.YELLOW, Color.GREEN, Color.CYAN, Color.BLUE, Color.MAGENTA, Color.RED
    )

    fun setColor(argb: Int) {
        if (mode != 0) return
        val hsv = FloatArray(3)
        Color.colorToHSV(argb, hsv)
        if (hsv[1] > 0.01f && hsv[2] > 0.01f) hue = hsv[0]
        sat = hsv[1]
        value = hsv[2]
        invalidate()
    }

    private fun current(): Int = Color.HSVToColor(floatArrayOf(hue, sat, value))

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        val w = MeasureSpec.getSize(widthMeasureSpec)
        setMeasuredDimension(w, dp(230f))
    }

    override fun onSizeChanged(w: Int, h: Int, ow: Int, oh: Int) {
        val pad = dp(6f).toFloat()
        val barH = dp(30f).toFloat()
        sv.set(pad, pad, w - pad, h - pad - barH - dp(16f))
        bar.set(pad, h - pad - barH, w - pad, h - pad)
    }

    override fun onDraw(canvas: Canvas) {
        val r = dp(14f).toFloat()
        val hueColor = Color.HSVToColor(floatArrayOf(hue, 1f, 1f))
        val h = LinearGradient(sv.left, 0f, sv.right, 0f, Color.WHITE, hueColor, Shader.TileMode.CLAMP)
        val v = LinearGradient(0f, sv.top, 0f, sv.bottom, Color.WHITE, Color.BLACK, Shader.TileMode.CLAMP)
        paint.shader = ComposeShader(h, v, PorterDuff.Mode.MULTIPLY)
        canvas.drawRoundRect(sv, r, r, paint)
        paint.shader = LinearGradient(bar.left, 0f, bar.right, 0f, hueColors, null, Shader.TileMode.CLAMP)
        canvas.drawRoundRect(bar, bar.height() / 2f, bar.height() / 2f, paint)
        paint.shader = null

        val sx = sv.left + sat * sv.width()
        val sy = sv.top + (1f - value) * sv.height()
        handle(canvas, sx, sy, current())
        val hx = bar.left + hue / 360f * bar.width()
        handle(canvas, hx, bar.centerY(), hueColor)
    }

    private fun handle(c: Canvas, x: Float, y: Float, color: Int) {
        paint.style = Paint.Style.FILL
        paint.color = Color.parseColor("#66000000")
        c.drawCircle(x, y + dp(1.5f), dp(13f).toFloat(), paint)
        paint.color = Color.WHITE
        c.drawCircle(x, y, dp(13f).toFloat(), paint)
        paint.color = color
        c.drawCircle(x, y, dp(10f).toFloat(), paint)
    }

    private fun apply(ev: MotionEvent, isFinal: Boolean) {
        if (mode == 1) {
            sat = ((ev.x - sv.left) / sv.width()).coerceIn(0f, 1f)
            value = 1f - ((ev.y - sv.top) / sv.height()).coerceIn(0f, 1f)
        } else if (mode == 2) {
            hue = (((ev.x - bar.left) / bar.width()).coerceIn(0f, 1f)) * 359.99f
        }
        invalidate()
        onColor?.invoke(current(), isFinal)
    }

    override fun onTouchEvent(ev: MotionEvent): Boolean {
        when (ev.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                parent?.requestDisallowInterceptTouchEvent(true)
                mode = if (ev.y >= bar.top - dp(8f)) 2 else 1
                apply(ev, false)
            }
            MotionEvent.ACTION_MOVE -> if (mode != 0) apply(ev, false)
            MotionEvent.ACTION_UP -> {
                if (mode != 0) apply(ev, true)
                mode = 0
            }
            MotionEvent.ACTION_CANCEL -> mode = 0
        }
        return true
    }
}

class ColorPanelView(context: Context, private val onEyedropper: () -> Unit) : ScrollView(context) {
    private val picker = ColorPickerView(context)
    private val hex = label(context, "#000000", 15f, T.TEXT, true)
    private val swatch = View(context)
    private val recentRow = LinearLayout(context)
    private val palette = intArrayOf(
        0xFF000000.toInt(), 0xFF3A3A3A.toInt(), 0xFF7A7A7A.toInt(), 0xFFBDBDBD.toInt(), 0xFFFFFFFF.toInt(), 0xFF7B4A2D.toInt(),
        0xFFC68642.toInt(), 0xFFF1C27D.toInt(), 0xFFFFDFC4.toInt(), 0xFFE53935.toInt(), 0xFFFB8C00.toInt(), 0xFFFDD835.toInt(),
        0xFF7CB342.toInt(), 0xFF00897B.toInt(), 0xFF039BE5.toInt(), 0xFF3949AB.toInt(), 0xFF8E24AA.toInt(), 0xFFD81B60.toInt(),
        0xFFFF8A80.toInt(), 0xFFFFD180.toInt(), 0xFFCCFF90.toInt(), 0xFF84FFFF.toInt(), 0xFF82B1FF.toInt(), 0xFFB388FF.toInt()
    )

    init {
        isVerticalScrollBarEnabled = false
        val col = LinearLayout(context)
        col.orientation = LinearLayout.VERTICAL
        col.setPadding(dp(14f), dp(4f), dp(14f), dp(14f))
        addView(col, FrameLayout.LayoutParams(MATCH, WRAP))
        col.addView(picker, lp(MATCH, dp(230f)))
        picker.onColor = { argb, isFinal ->
            Native.setColor(argb)
            updateHex(argb)
            if (isFinal) {
                Native.addRecent(argb)
                refreshRecents()
            }
        }
        val row = LinearLayout(context)
        row.orientation = LinearLayout.HORIZONTAL
        row.gravity = Gravity.CENTER_VERTICAL
        swatch.background = roundBg(Color.BLACK, dp(12f).toFloat(), Color.parseColor("#44FFFFFF"), 1)
        row.addView(swatch, lp(dp(40f), dp(40f)))
        row.addView(hex, lpw(0, WRAP, 1f).also { it.leftMargin = dp(12f) })
        row.addView(IconButton(context, R.drawable.ic_eyedropper, 44f, 24f).tip("Conta-gotas").style(false).click { onEyedropper() }, lp(dp(44f), dp(44f)))
        row.addView(IconButton(context, R.drawable.ic_paper, 44f, 24f).tip("Usar a cor como cor do papel").click {
            Native.setPaperColor(Native.getColor())
        }, lp(dp(44f), dp(44f)))
        col.addView(row, lp(MATCH, WRAP).also { it.topMargin = dp(4f) })

        col.addView(label(context, "Recentes", 12f, T.DIM, true), lp(WRAP, WRAP).also { it.topMargin = dp(14f); it.bottomMargin = dp(8f) })
        val hs = HorizontalScrollView(context)
        hs.isHorizontalScrollBarEnabled = false
        recentRow.orientation = LinearLayout.HORIZONTAL
        hs.addView(recentRow)
        col.addView(hs, lp(MATCH, dp(40f)))

        col.addView(label(context, "Paleta", 12f, T.DIM, true), lp(WRAP, WRAP).also { it.topMargin = dp(12f); it.bottomMargin = dp(8f) })
        for (r in 0 until 2) {
            val line = LinearLayout(context)
            line.orientation = LinearLayout.HORIZONTAL
            for (c in 0 until 12) {
                val color = palette[r * 12 + c]
                val v = View(context)
                v.background = roundBg(color, dp(9f).toFloat(), Color.parseColor("#33FFFFFF"), 1)
                v.setOnClickListener { pick(color) }
                line.addView(v, lpw(0, dp(28f), 1f).also { it.setMargins(dp(2f), dp(2f), dp(2f), dp(2f)) })
            }
            col.addView(line, lp(MATCH, WRAP))
        }
    }

    private fun pick(argb: Int) {
        Native.setColor(argb)
        Native.addRecent(argb)
        refresh()
    }

    private fun updateHex(argb: Int) {
        hex.text = String.format("#%06X", argb and 0xFFFFFF)
        swatch.background = roundBg(argb, dp(12f).toFloat(), Color.parseColor("#44FFFFFF"), 1)
    }

    private fun refreshRecents() {
        recentRow.removeAllViews()
        val rs = Native.recents()
        if (rs.isEmpty()) {
            recentRow.addView(label(context, "As cores usadas aparecem aqui", 12f, T.DIM))
            return
        }
        for (c in rs) {
            val v = View(context)
            v.background = roundBg(c, dp(10f).toFloat(), Color.parseColor("#44FFFFFF"), 1)
            v.setOnClickListener { pick(c) }
            recentRow.addView(v, lp(dp(34f), dp(34f)).also { it.rightMargin = dp(8f) })
        }
    }

    fun refresh() {
        val c = Native.getColor()
        picker.setColor(c)
        updateHex(c)
        refreshRecents()
    }
}

// ====================================================================== lista de pincéis

class BrushListView(context: Context) : LinearLayout(context) {
    private var eraser = false
    private var source = 0
    private val groupSel = intArrayOf(0, 0)
    private var groups: Array<String> = emptyArray()
    private var ids: IntArray = IntArray(0)
    private val srcRow = LinearLayout(context)
    private val groupRow = LinearLayout(context)
    private val list = ListView(context)

    private inner class Adapter : BaseAdapter() {
        override fun getCount(): Int = ids.size
        override fun getItem(position: Int): Any = ids[position]
        override fun getItemId(position: Int): Long = ids[position].toLong()
        override fun getView(position: Int, convertView: View?, parent: ViewGroup?): View {
            val row = (convertView as? BrushRow) ?: BrushRow(context)
            row.bind(ids[position], ids[position] == Native.currentBrush(eraser))
            return row
        }
    }

    private class BrushRow(ctx: Context) : LinearLayout(ctx) {
        private val img = ImageView(ctx)
        private val name = label(ctx, "", 14f, T.TEXT, true)
        private val tag = label(ctx, "", 11f, T.DIM)

        init {
            orientation = HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(dp(10f), dp(8f), dp(10f), dp(8f))
            img.scaleType = ImageView.ScaleType.FIT_CENTER
            img.background = roundBg(Color.parseColor("#0E1014"), dp(10f).toFloat())
            addView(img, lp(dp(136f), dp(40f)))
            val col = LinearLayout(ctx)
            col.orientation = VERTICAL
            name.maxLines = 1
            name.ellipsize = TextUtils.TruncateAt.END
            tag.maxLines = 1
            tag.ellipsize = TextUtils.TruncateAt.END
            col.addView(name)
            col.addView(tag, lp(WRAP, WRAP).also { it.topMargin = dp(4f) })
            addView(col, lpw(0, WRAP, 1f).also { it.leftMargin = dp(12f) })
            layoutParams = AbsListViewLP()
        }

        private fun AbsListViewLP() = android.widget.AbsListView.LayoutParams(MATCH, dp(64f))

        fun bind(idx: Int, selected: Boolean) {
            background = if (selected) roundBg(T.ACCENT_DIM, dp(14f).toFloat(), T.ACCENT, dp(2f)) else ripple(roundBg(Color.TRANSPARENT, dp(14f).toFloat()))
            name.text = Native.brushName(idx)
            val f = Native.brushFlags(idx)
            var t = Native.brushGroup(idx)
            if (f and 2 != 0) t += " · mistura"
            if (f and 4 != 0) t += " · luz"
            if (f and 8 != 0) t += " · animado"
            tag.text = t
            val bmp = Previews.get(idx)
            if (bmp != null) img.setImageBitmap(bmp) else img.setImageDrawable(null)
        }
    }

    init {
        orientation = VERTICAL
        srcRow.orientation = HORIZONTAL
        addView(srcRow, lp(MATCH, WRAP).also { it.setMargins(dp(12f), dp(2f), dp(12f), dp(8f)) })
        val hs = HorizontalScrollView(context)
        hs.isHorizontalScrollBarEnabled = false
        groupRow.orientation = HORIZONTAL
        hs.addView(groupRow)
        addView(hs, lp(MATCH, WRAP).also { it.setMargins(dp(12f), 0, dp(12f), dp(6f)) })
        list.divider = null
        list.selector = ColorDrawableTransparent()
        list.adapter = Adapter()
        list.setOnItemClickListener { _, _, position, _ ->
            Native.selectBrush(eraser, ids[position])
            (list.adapter as BaseAdapter).notifyDataSetChanged()
        }
        addView(list, lp(MATCH, 0).also { it.weight = 1f })
        Previews.listener = { (list.adapter as BaseAdapter).notifyDataSetChanged() }
    }

    private fun ColorDrawableTransparent() = android.graphics.drawable.ColorDrawable(Color.TRANSPARENT)

    fun bind(eraserTool: Boolean) {
        eraser = eraserTool
        rebuild()
    }

    private fun rebuild() {
        srcRow.removeAllViews()
        val names = arrayOf("MyPaint", "AnimBrush")
        for (i in 0 until 2) {
            val c = Chip(context, names[i]).style(i == source)
            c.setOnClickListener { source = i; rebuild() }
            srcRow.addView(c, lp(WRAP, dp(36f)).also { it.rightMargin = dp(8f) })
        }
        val all = Native.brushGroups(source)
        val usable = ArrayList<String>()
        for (g in all) if (Native.brushesIn(source, g, eraser).isNotEmpty()) usable.add(g)
        groups = usable.toTypedArray()
        if (groupSel[source] >= groups.size) groupSel[source] = 0
        groupRow.removeAllViews()
        for (i in groups.indices) {
            val c = Chip(context, groups[i], true).style(i == groupSel[source])
            c.setOnClickListener { groupSel[source] = i; rebuild() }
            groupRow.addView(c, lp(WRAP, dp(32f)).also { it.rightMargin = dp(6f) })
        }
        ids = if (groups.isEmpty()) IntArray(0) else Native.brushesIn(source, groups[groupSel[source]], eraser)
        (list.adapter as BaseAdapter).notifyDataSetChanged()
        list.setSelection(0)
    }
}

// ====================================================================== ajustes

class SettingsView(context: Context) : ScrollView(context) {
    private val col = LinearLayout(context)

    init {
        isVerticalScrollBarEnabled = false
        col.orientation = LinearLayout.VERTICAL
        col.setPadding(dp(16f), dp(4f), dp(16f), dp(16f))
        addView(col, FrameLayout.LayoutParams(MATCH, WRAP))
    }

    private fun sliderRow(title: String, valueText: () -> String, initial: Float, onChange: (Float) -> Unit, onStart: (() -> Unit)? = null) {
        val head = LinearLayout(context)
        head.orientation = LinearLayout.HORIZONTAL
        head.addView(label(context, title, 13f, T.DIM, true), lpw(0, WRAP, 1f))
        val v = label(context, valueText(), 13f, T.TEXT, true)
        head.addView(v)
        col.addView(head, lp(MATCH, WRAP).also { it.topMargin = dp(10f) })
        val s = SliderView(context)
        s.value = initial
        s.onChange = {
            onChange(it)
            v.text = valueText()
        }
        s.onStart = onStart
        col.addView(s, lp(MATCH, dp(40f)))
    }

    fun bind(tool: Int) {
        col.removeAllViews()
        if (tool == 2) {
            var tol = Native.getFill(0)
            sliderRow("Tolerância", { "${(tol * 100).toInt()}%" }, tol, { tol = it; Native.setFill(0, it) })
            var grow = Native.getFill(1)
            sliderRow("Expandir borda", { "${grow.toInt()} px" }, grow / 8f, { grow = it * 8f; Native.setFill(1, grow) })
            val row = LinearLayout(context)
            row.orientation = LinearLayout.HORIZONTAL
            val c1 = Chip(context, "Camada ativa")
            val c2 = Chip(context, "Todas as camadas")
            fun paint() {
                val all = Native.getFlag(4)
                c1.style(!all)
                c2.style(all)
            }
            c1.setOnClickListener { Native.setFlag(4, false); paint() }
            c2.setOnClickListener { Native.setFlag(4, true); paint() }
            paint()
            row.addView(c1, lp(WRAP, dp(38f)).also { it.rightMargin = dp(8f) })
            row.addView(c2, lp(WRAP, dp(38f)))
            col.addView(label(context, "Amostrar cores de", 13f, T.DIM, true), lp(WRAP, WRAP).also { it.topMargin = dp(18f); it.bottomMargin = dp(8f) })
            col.addView(row)
            return
        }
        val eraser = tool == 1
        val brush = Native.currentBrush(eraser)
        val flags = Native.brushFlags(brush)
        val myPaint = flags and 16 == 0
        var d = Native.getBrushValue(eraser, 0)
        sliderRow("Tamanho", { "${Math.round(d)} px" }, Sizes.toSlider(d), { d = Sizes.toSize(it); Native.setBrushValue(eraser, 0, d) })
        var op = Native.getBrushValue(eraser, 1)
        sliderRow("Opacidade", { "${Math.round(op * 100)}%" }, op, { op = it; Native.setBrushValue(eraser, 1, it) })
        if (myPaint) {
            var hd = Native.getBrushValue(eraser, 2)
            sliderRow("Dureza", { if (hd < 0) "do pincel" else "${Math.round(hd * 100)}%" }, if (hd < 0) 0.8f else hd, { hd = it; Native.setBrushValue(eraser, 2, it) })
            var sm = Native.getBrushValue(eraser, 3)
            sliderRow("Suavização do traço", { "${Math.round(sm * 100)}%" }, sm, { sm = it; Native.setBrushValue(eraser, 3, it) })
        }
        // atalhos de desenho (so icones)
        val row = LinearLayout(context)
        row.orientation = LinearLayout.HORIZONTAL
        row.gravity = Gravity.CENTER_VERTICAL
        val mirror = IconButton(context, R.drawable.ic_mirror, 48f, 26f).tip("Simetria vertical")
        val finger = IconButton(context, R.drawable.ic_finger, 48f, 26f).tip("Desenhar com o dedo (desligado: só caneta)")
        fun sync() {
            mirror.style(Native.getFlag(0))
            finger.style(Native.getFlag(2))
        }
        mirror.setOnClickListener { Native.setFlag(0, !Native.getFlag(0)); sync() }
        finger.setOnClickListener { Native.setFlag(2, !Native.getFlag(2)); sync() }
        sync()
        row.addView(mirror, lp(dp(48f), dp(48f)))
        row.addView(finger, lp(dp(48f), dp(48f)).also { it.leftMargin = dp(8f) })
        col.addView(row, lp(MATCH, WRAP).also { it.topMargin = dp(16f) })

        val desc = Native.brushDescription(brush)
        val text = if (desc.isNotEmpty()) desc else Native.brushName(brush) + " · " + Native.brushGroup(brush)
        col.addView(label(context, text, 12f, T.DIM), lp(MATCH, WRAP).also { it.topMargin = dp(14f) })
    }
}

// ====================================================================== painel de ferramenta

class ToolPanel(context: Context, private val closeAll: () -> Unit, private val onEyedropper: () -> Unit) : LinearLayout(context) {
    private val tabsRow = LinearLayout(context)
    private val host = FrameLayout(context)
    private val brushes = BrushListView(context)
    private val settings = SettingsView(context)
    private val color = ColorPanelView(context, onEyedropper)
    private var tool = 0
    private var tab = 0   // 0 pinceis, 1 ajustes, 2 cor

    init {
        orientation = VERTICAL
        background = roundBg(T.SURFACE, dp(28f).toFloat())
        elevation = dp(12f).toFloat()
        tabsRow.orientation = HORIZONTAL
        tabsRow.gravity = Gravity.CENTER_VERTICAL
        tabsRow.setPadding(dp(10f), dp(8f), dp(8f), dp(6f))
        addView(tabsRow, lp(MATCH, WRAP))
        addView(host, lp(MATCH, 0).also { it.weight = 1f })
        host.addView(brushes, FrameLayout.LayoutParams(MATCH, MATCH))
        host.addView(settings, FrameLayout.LayoutParams(MATCH, MATCH))
        host.addView(color, FrameLayout.LayoutParams(MATCH, MATCH))
    }

    fun open(toolId: Int, tabId: Int) {
        tool = toolId
        tab = if (toolId == 2 && tabId == 0) 1 else tabId
        refresh()
    }

    fun currentTab() = tab

    fun refresh() {
        tabsRow.removeAllViews()
        val first = if (tool == 2) 1 else 0
        val icons = intArrayOf(if (tool == 1) R.drawable.ic_eraser else R.drawable.ic_brush_variant, R.drawable.ic_tune, R.drawable.ic_palette)
        val tips = arrayOf(if (tool == 1) "Borrachas" else "Pincéis", "Ajustes", "Cor")
        for (i in first until 3) {
            val b = IconButton(context, icons[i], 46f, 24f).tip(tips[i]).style(tab == i)
            b.setOnClickListener {
                tab = i
                refresh()
            }
            tabsRow.addView(b, lp(dp(46f), dp(46f)).also { it.rightMargin = dp(6f) })
        }
        val sp = View(context)
        tabsRow.addView(sp, lpw(0, 1, 1f))
        tabsRow.addView(IconButton(context, R.drawable.ic_close, 44f, 22f).tip("Fechar").click { closeAll() }, lp(dp(44f), dp(44f)))
        brushes.visibility = if (tab == 0) View.VISIBLE else View.GONE
        settings.visibility = if (tab == 1) View.VISIBLE else View.GONE
        color.visibility = if (tab == 2) View.VISIBLE else View.GONE
        when (tab) {
            0 -> brushes.bind(tool == 1)
            1 -> settings.bind(tool)
            else -> color.refresh()
        }
    }

    fun tick() {
        if (tab == 2) color.refresh()
    }
}

// ====================================================================== camadas

class LayersPanel(context: Context, private val closeAll: () -> Unit) : LinearLayout(context) {
    private val list = ListView(context)
    private val opacity = SliderView(context)
    private val opacityText = label(context, "100%", 13f, T.TEXT, true)
    private val blendChip = Chip(context, "Normal", true)
    private val alphaBtn = IconButton(context, R.drawable.ic_checker, 42f, 22f).tip("Bloquear transparência (alfa)")
    private val lockBtn = IconButton(context, R.drawable.ic_lock, 42f, 22f).tip("Bloquear camada")
    private var layerTotal = 0

    private inner class Adapter : BaseAdapter() {
        override fun getCount(): Int = layerTotal
        override fun getItem(position: Int): Any = position
        override fun getItemId(position: Int): Long = position.toLong()
        override fun getView(position: Int, convertView: View?, parent: ViewGroup?): View {
            val row = (convertView as? LayerRow) ?: LayerRow(context)
            row.bind(layerTotal - 1 - position)
            return row
        }
    }

    private class LayerRow(ctx: Context) : LinearLayout(ctx) {
        private val eye = IconButton(ctx, R.drawable.ic_eye, 40f, 22f)
        private val thumb = ImageView(ctx)
        private val name = label(ctx, "", 14f, T.TEXT, true)
        private val info = label(ctx, "", 11f, T.DIM)
        private val lock = ImageView(ctx)

        init {
            orientation = HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(dp(8f), dp(6f), dp(12f), dp(6f))
            addView(eye, lp(dp(40f), dp(40f)))
            thumb.scaleType = ImageView.ScaleType.FIT_CENTER
            thumb.background = roundBg(Color.parseColor("#C9CCD4"), dp(8f).toFloat())
            thumb.setPadding(dp(2f), dp(2f), dp(2f), dp(2f))
            addView(thumb, lp(dp(60f), dp(46f)).also { it.leftMargin = dp(6f) })
            val col = LinearLayout(ctx)
            col.orientation = VERTICAL
            name.maxLines = 1
            name.ellipsize = TextUtils.TruncateAt.END
            col.addView(name)
            col.addView(info, lp(WRAP, WRAP).also { it.topMargin = dp(4f) })
            addView(col, lpw(0, WRAP, 1f).also { it.leftMargin = dp(12f) })
            lock.setImageResource(R.drawable.ic_lock)
            lock.setColorFilter(T.DIM)
            addView(lock, lp(dp(18f), dp(18f)))
            layoutParams = android.widget.AbsListView.LayoutParams(MATCH, dp(62f))
        }

        fun bind(i: Int) {
            val active = i == Native.activeLayer()
            background = if (active) roundBg(T.ACCENT_DIM, dp(14f).toFloat(), T.ACCENT, dp(2f)) else ripple(roundBg(Color.TRANSPARENT, dp(14f).toFloat()))
            val flags = Native.layerFlags(i)
            val visible = flags and 1 != 0
            eye.setIcon(if (visible) R.drawable.ic_eye else R.drawable.ic_eye_off)
            eye.style(false, true, if (visible) T.TEXT else T.DIM)
            eye.setOnClickListener { Native.layerOp(8, i, 0f); (parent as? ListView)?.let { (it.adapter as BaseAdapter).notifyDataSetChanged() } }
            name.text = Native.layerName(i)
            info.text = Native.layerBlend(i) + " · " + Math.round(Native.layerOpacity(i) * 100) + "%"
            lock.visibility = if (flags and 6 != 0) View.VISIBLE else View.GONE
            val px = Native.layerThumb(i, 120, 92)
            if (px != null) {
                val bmp = Bitmap.createBitmap(120, 92, Bitmap.Config.ARGB_8888)
                bmp.setPixels(px, 0, 120, 0, 0, 120, 92)
                thumb.setImageBitmap(bmp)
            }
        }
    }

    init {
        orientation = VERTICAL
        background = roundBg(T.SURFACE, dp(28f).toFloat())
        elevation = dp(12f).toFloat()

        val head = LinearLayout(context)
        head.orientation = HORIZONTAL
        head.gravity = Gravity.CENTER_VERTICAL
        head.setPadding(dp(10f), dp(8f), dp(8f), dp(6f))
        class A(val icon: Int, val tip: String, val op: Int)
        val acts = listOf(
            A(R.drawable.ic_layer_add, "Nova camada", 0), A(R.drawable.ic_duplicate, "Duplicar", 1), A(R.drawable.ic_merge, "Mesclar com a de baixo", 3),
            A(R.drawable.ic_up, "Subir", 4), A(R.drawable.ic_down, "Descer", 5), A(R.drawable.ic_clear, "Limpar camada", 6), A(R.drawable.ic_trash, "Excluir", 2)
        )
        for (a in acts) {
            head.addView(IconButton(context, a.icon, 42f, 22f).tip(a.tip).click {
                Native.layerOp(a.op, 0, 0f)
                refresh()
            }, lp(dp(42f), dp(42f)))
        }
        head.addView(View(context), lpw(0, 1, 1f))
        head.addView(IconButton(context, R.drawable.ic_close, 44f, 22f).tip("Fechar").click { closeAll() }, lp(dp(44f), dp(44f)))
        addView(head, lp(MATCH, WRAP))

        list.divider = null
        list.selector = android.graphics.drawable.ColorDrawable(Color.TRANSPARENT)
        list.adapter = Adapter()
        list.setOnItemClickListener { _, _, position, _ ->
            Native.layerOp(7, layerTotal - 1 - position, 0f)
            refresh()
        }
        addView(list, lp(MATCH, 0).also { it.weight = 1f; it.setMargins(dp(8f), 0, dp(8f), 0) })

        val props = LinearLayout(context)
        props.orientation = VERTICAL
        props.setPadding(dp(16f), dp(6f), dp(12f), dp(12f))
        val ohead = LinearLayout(context)
        ohead.orientation = HORIZONTAL
        ohead.gravity = Gravity.CENTER_VERTICAL
        ohead.addView(IconButton(context, R.drawable.ic_opacity, 28f, 20f).tip("Opacidade da camada"), lp(dp(28f), dp(28f)))
        ohead.addView(opacity, lpw(0, dp(40f), 1f))
        ohead.addView(opacityText, lp(dp(44f), WRAP))
        props.addView(ohead, lp(MATCH, WRAP))
        opacity.onStart = { Native.layerOp(13, 0, 0f) }
        opacity.onChange = {
            Native.layerOp(12, Native.activeLayer(), it)
            opacityText.text = "${Math.round(it * 100)}%"
        }
        opacity.onEnd = { refresh() }
        val brow = LinearLayout(context)
        brow.orientation = HORIZONTAL
        brow.gravity = Gravity.CENTER_VERTICAL
        blendChip.setOnClickListener {
            Native.layerOp(11, Native.activeLayer(), 0f)
            refresh()
        }
        brow.addView(blendChip, lp(WRAP, dp(36f)))
        brow.addView(View(context), lpw(0, 1, 1f))
        alphaBtn.setOnClickListener { Native.layerOp(10, Native.activeLayer(), 0f); refresh() }
        lockBtn.setOnClickListener { Native.layerOp(9, Native.activeLayer(), 0f); refresh() }
        brow.addView(alphaBtn, lp(dp(42f), dp(42f)))
        brow.addView(lockBtn, lp(dp(42f), dp(42f)))
        props.addView(brow, lp(MATCH, WRAP).also { it.topMargin = dp(4f) })
        addView(props, lp(MATCH, WRAP))
    }

    fun refresh() {
        layerTotal = Native.layerCount()
        val a = Native.activeLayer()
        val op = Native.layerOpacity(a)
        opacity.value = op
        opacityText.text = "${Math.round(op * 100)}%"
        blendChip.text = Native.layerBlend(a)
        val flags = Native.layerFlags(a)
        alphaBtn.style(flags and 4 != 0)
        lockBtn.style(flags and 2 != 0)
        (list.adapter as BaseAdapter).notifyDataSetChanged()
    }
}
