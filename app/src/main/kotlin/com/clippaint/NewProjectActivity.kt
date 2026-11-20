package com.clippaint

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.os.Bundle
import android.text.Editable
import android.text.InputType
import android.text.TextWatcher
import android.view.Gravity
import android.view.View
import android.widget.EditText
import android.widget.FrameLayout
import android.widget.HorizontalScrollView
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView

/** Tela de criacao de projeto: nome, tamanho, resolucao (dpi) e fundo. */
class NewProjectActivity : Activity() {

    private class Preset(val label: String, val w: Float, val h: Float, val mm: Boolean)

    private val presets = listOf(
        Preset("Quadrado", 2048f, 2048f, false),
        Preset("Retrato", 1536f, 2048f, false),
        Preset("Paisagem", 2048f, 1536f, false),
        Preset("HD 16:9", 1920f, 1080f, false),
        Preset("A4", 210f, 297f, true),
        Preset("A5", 148f, 210f, true),
        Preset("Personalizado", 0f, 0f, false)
    )
    private val dpis = intArrayOf(72, 150, 300, 600)
    private val paperColors = intArrayOf(
        Color.WHITE, Color.parseColor("#F5EFE0"), Color.parseColor("#DADCE3"),
        Color.parseColor("#1A1C22"), Color.BLACK, Color.parseColor("#BFD9FF"), Color.parseColor("#FFD9D9")
    )

    private var presetIdx = 2
    private var dpi = 300
    private var bgMode = 0          // 0 branco, 1 transparente, 2 cor
    private var bgColor = Color.WHITE
    private var customUnitCm = false

    private lateinit var nameEdit: EditText
    private lateinit var wEdit: EditText
    private lateinit var hEdit: EditText
    private lateinit var preview: AspectPreview
    private lateinit var info: TextView
    private lateinit var createBtn: TextView
    private lateinit var presetRow: LinearLayout
    private lateinit var dpiRow: LinearLayout
    private lateinit var customBox: LinearLayout
    private lateinit var unitRow: LinearLayout
    private lateinit var bgRow: LinearLayout
    private lateinit var colorRow: LinearLayout
    private lateinit var warn: TextView
    private var curW = 2048
    private var curH = 1536

    /** Retangulo proporcional ao tamanho escolhido. */
    private class AspectPreview(ctx: Context) : View(ctx) {
        var w = 1
        var h = 1
        var fill = Color.WHITE
        var transparent = false
        private val p = Paint(Paint.ANTI_ALIAS_FLAG)
        private val r = RectF()
        override fun onDraw(c: Canvas) {
            val pad = dp(18f).toFloat()
            val aw = width - 2 * pad
            val ah = height - 2 * pad
            val s = Math.min(aw / w, ah / h)
            val rw = w * s
            val rh = h * s
            r.set((width - rw) / 2f, (height - rh) / 2f, (width + rw) / 2f, (height + rh) / 2f)
            p.color = Color.parseColor("#55000000")
            c.drawRoundRect(RectF(r.left, r.top + dp(4f), r.right, r.bottom + dp(4f)), dp(6f).toFloat(), dp(6f).toFloat(), p)
            p.color = fill
            c.drawRoundRect(r, dp(6f).toFloat(), dp(6f).toFloat(), p)
            if (transparent) {
                c.save()
                c.clipRect(r)
                val cell = dp(12f).toFloat()
                var y = r.top
                var row = 0
                while (y < r.bottom) {
                    var x = r.left
                    var col = row
                    while (x < r.right) {
                        p.color = if (col % 2 == 0) Color.parseColor("#C9CCD4") else Color.parseColor("#EEF0F4")
                        c.drawRect(x, y, x + cell, y + cell, p)
                        x += cell
                        col++
                    }
                    y += cell
                    row++
                }
                c.restore()
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        edgeToEdge()
        val root = FrameLayout(this)
        root.setBackgroundColor(T.BG)

        val scroll = ScrollView(this)
        scroll.isVerticalScrollBarEnabled = false
        val col = LinearLayout(this)
        col.orientation = LinearLayout.VERTICAL
        scroll.addView(col, FrameLayout.LayoutParams(MATCH, WRAP))
        root.addView(scroll, FrameLayout.LayoutParams(MATCH, MATCH).also { it.bottomMargin = dp(84f) })

        // topo
        val top = LinearLayout(this)
        top.orientation = LinearLayout.HORIZONTAL
        top.gravity = Gravity.CENTER_VERTICAL
        val back = IconButton(this, R.drawable.ic_back).tip("Voltar").click { finish() }
        top.addView(back, lp(dp(44f), dp(44f)))
        top.addView(label(this, "Novo projeto", 22f, T.TEXT, true), lp(WRAP, WRAP).also { it.leftMargin = dp(10f) })
        col.addView(top)

        // pre-visualizacao
        preview = AspectPreview(this)
        preview.background = roundBg(T.SURFACE, dp(24f).toFloat())
        col.addView(preview, lp(MATCH, dp(200f)).also { it.topMargin = dp(14f) })
        info = label(this, "", 13f, T.DIM)
        info.gravity = Gravity.CENTER
        col.addView(info, lp(MATCH, WRAP).also { it.topMargin = dp(10f) })

        // nome
        col.addView(section("Nome do projeto"))
        nameEdit = styledEdit(ProjectStore.nextDefaultName(this), InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_FLAG_CAP_SENTENCES)
        nameEdit.setSelectAllOnFocus(true)
        col.addView(nameEdit, lp(MATCH, dp(52f)))

        // tamanho
        col.addView(section("Tamanho"))
        val hs = HorizontalScrollView(this)
        hs.isHorizontalScrollBarEnabled = false
        presetRow = LinearLayout(this)
        presetRow.orientation = LinearLayout.HORIZONTAL
        hs.addView(presetRow)
        col.addView(hs, lp(MATCH, WRAP))

        customBox = LinearLayout(this)
        customBox.orientation = LinearLayout.VERTICAL
        val inputs = LinearLayout(this)
        inputs.orientation = LinearLayout.HORIZONTAL
        inputs.gravity = Gravity.CENTER_VERTICAL
        wEdit = styledEdit("2048", InputType.TYPE_CLASS_NUMBER or InputType.TYPE_NUMBER_FLAG_DECIMAL)
        hEdit = styledEdit("1536", InputType.TYPE_CLASS_NUMBER or InputType.TYPE_NUMBER_FLAG_DECIMAL)
        inputs.addView(wEdit, lpw(0, dp(52f), 1f))
        inputs.addView(label(this, "×", 20f, T.DIM, true), lp(dp(34f), WRAP).also { })
        (inputs.getChildAt(1) as TextView).gravity = Gravity.CENTER
        inputs.addView(hEdit, lpw(0, dp(52f), 1f))
        customBox.addView(inputs, lp(MATCH, WRAP))
        unitRow = LinearLayout(this)
        unitRow.orientation = LinearLayout.HORIZONTAL
        customBox.addView(unitRow, lp(MATCH, WRAP).also { it.topMargin = dp(10f) })
        col.addView(customBox, lp(MATCH, WRAP).also { it.topMargin = dp(12f) })
        val watcher = object : TextWatcher {
            override fun beforeTextChanged(s: CharSequence?, st: Int, c: Int, a: Int) {}
            override fun onTextChanged(s: CharSequence?, st: Int, b: Int, c: Int) {}
            override fun afterTextChanged(s: Editable?) { recompute() }
        }
        wEdit.addTextChangedListener(watcher)
        hEdit.addTextChangedListener(watcher)

        // resolucao
        col.addView(section("Resolução"))
        dpiRow = LinearLayout(this)
        dpiRow.orientation = LinearLayout.HORIZONTAL
        col.addView(dpiRow, lp(MATCH, WRAP))
        col.addView(label(this, "Define o tamanho físico (A4, A5, cm) e é gravada nos PNG exportados.", 12f, T.DIM),
            lp(MATCH, WRAP).also { it.topMargin = dp(8f) })

        // fundo
        col.addView(section("Fundo"))
        bgRow = LinearLayout(this)
        bgRow.orientation = LinearLayout.HORIZONTAL
        col.addView(bgRow, lp(MATCH, WRAP))
        colorRow = LinearLayout(this)
        colorRow.orientation = LinearLayout.HORIZONTAL
        col.addView(colorRow, lp(MATCH, WRAP).also { it.topMargin = dp(10f) })

        warn = label(this, "", 13f, T.DANGER, true)
        col.addView(warn, lp(MATCH, WRAP).also { it.topMargin = dp(16f) })

        // botao criar fixo
        createBtn = label(this, "Criar projeto", 16f, Color.WHITE, true)
        createBtn.gravity = Gravity.CENTER
        createBtn.background = ripple(roundBg(T.ACCENT, dp(26f).toFloat()), 0x44FFFFFF)
        createBtn.setOnClickListener { create() }
        root.addView(createBtn, flp(MATCH, dp(54f), Gravity.BOTTOM).also { it.setMargins(dp(20f), 0, dp(20f), dp(16f)) })

        setContentView(root)
        root.setOnApplyWindowInsetsListener { _, insets ->
            col.setPadding(dp(20f) + insets.sysLeft(), dp(12f) + insets.sysTop(), dp(20f) + insets.sysRight(), dp(24f))
            (createBtn.layoutParams as FrameLayout.LayoutParams).setMargins(
                dp(20f) + insets.sysLeft(), 0, dp(20f) + insets.sysRight(), dp(16f) + insets.sysBottom()
            )
            createBtn.requestLayout()
            insets
        }
        root.requestApplyInsets()
        rebuild()
    }

    private fun section(text: String): TextView {
        val t = label(this, text, 15f, T.TEXT, true)
        t.layoutParams = lp(MATCH, WRAP).also { it.topMargin = dp(24f); it.bottomMargin = dp(10f) }
        return t
    }

    private fun styledEdit(text: String, type: Int): EditText {
        val e = EditText(this)
        e.setText(text)
        e.inputType = type
        e.setSingleLine(true)
        e.setTextColor(T.TEXT)
        e.setHintTextColor(T.DIM)
        e.textSize = 16f
        e.background = roundBg(T.SURFACE2, dp(16f).toFloat(), Color.parseColor("#22FFFFFF"), 1)
        e.setPadding(dp(16f), 0, dp(16f), 0)
        return e
    }

    private fun chipRow(row: LinearLayout, labels: List<String>, selected: Int, onPick: (Int) -> Unit) {
        row.removeAllViews()
        for (i in labels.indices) {
            val c = Chip(this, labels[i]).style(i == selected)
            c.setOnClickListener { onPick(i) }
            row.addView(c, lp(WRAP, dp(40f)).also { it.rightMargin = dp(8f) })
        }
    }

    private fun rebuild() {
        chipRow(presetRow, presets.map { it.label }, presetIdx) { presetIdx = it; rebuild() }
        chipRow(dpiRow, dpis.map { "$it dpi" }, dpis.indexOf(dpi)) { dpi = dpis[it]; rebuild() }
        customBox.visibility = if (presets[presetIdx].label == "Personalizado") View.VISIBLE else View.GONE
        chipRow(unitRow, listOf("Pixels", "Centímetros"), if (customUnitCm) 1 else 0) { customUnitCm = it == 1; rebuild() }

        bgRow.removeAllViews()
        val opts = listOf("Branco" to Color.WHITE, "Transparente" to 0, "Cor" to bgColor)
        for (i in opts.indices) {
            val box = LinearLayout(this)
            box.orientation = LinearLayout.VERTICAL
            box.gravity = Gravity.CENTER
            box.background = ripple(roundBg(if (i == bgMode) T.ACCENT_DIM else T.SURFACE2, dp(18f).toFloat(),
                if (i == bgMode) T.ACCENT else Color.TRANSPARENT, dp(2f)))
            val dot = FrameLayout(this)
            val ic = ImageView(this)
            if (i == 1) {
                dot.background = roundBg(Color.parseColor("#C9CCD4"), dp(14f).toFloat())
                ic.setImageResource(R.drawable.ic_checker)
                ic.setColorFilter(Color.parseColor("#6B7280"))
                dot.addView(ic, flp(dp(22f), dp(22f), Gravity.CENTER))
            } else {
                dot.background = roundBg(opts[i].second, dp(14f).toFloat(), Color.parseColor("#44FFFFFF"), 1)
            }
            box.addView(dot, lp(dp(40f), dp(40f)))
            box.addView(label(this, opts[i].first, 12f, T.TEXT, true), lp(WRAP, WRAP).also { it.topMargin = dp(8f) })
            box.setOnClickListener {
                bgMode = i
                rebuild()
            }
            bgRow.addView(box, lpw(0, dp(92f), 1f).also { it.rightMargin = if (i < 2) dp(10f) else 0 })
        }
        colorRow.removeAllViews()
        colorRow.visibility = if (bgMode == 2) View.VISIBLE else View.GONE
        for (c in paperColors) {
            val sw = View(this)
            sw.background = roundBg(c, dp(15f).toFloat(), if (c == bgColor) T.ACCENT else Color.parseColor("#33FFFFFF"), dp(if (c == bgColor) 3f else 1f))
            sw.setOnClickListener { bgColor = c; rebuild() }
            colorRow.addView(sw, lp(dp(34f), dp(34f)).also { it.rightMargin = dp(10f) })
        }
        recompute()
    }

    private fun parse(e: EditText): Float {
        return e.text.toString().replace(',', '.').toFloatOrNull() ?: 0f
    }

    private fun recompute() {
        val pr = presets[presetIdx]
        if (pr.label == "Personalizado") {
            val a = parse(wEdit)
            val b = parse(hEdit)
            curW = if (customUnitCm) Math.round(a / 2.54f * dpi) else Math.round(a)
            curH = if (customUnitCm) Math.round(b / 2.54f * dpi) else Math.round(b)
        } else if (pr.mm) {
            curW = Math.round(pr.w / 25.4f * dpi)
            curH = Math.round(pr.h / 25.4f * dpi)
        } else {
            curW = pr.w.toInt()
            curH = pr.h.toInt()
        }
        val okSize = curW in 16..8192 && curH in 16..8192
        val tooBig = okSize && curW.toLong() * curH > 16_777_216L
        val valid = okSize && !tooBig
        preview.w = Math.max(curW, 1)
        preview.h = Math.max(curH, 1)
        preview.fill = if (bgMode == 0) Color.WHITE else bgColor
        preview.transparent = bgMode == 1
        preview.invalidate()
        if (okSize) {
            val cmW = curW / dpi.toFloat() * 2.54f
            val cmH = curH / dpi.toFloat() * 2.54f
            info.text = "$curW × $curH px  ·  ${String.format("%.1f", cmW)} × ${String.format("%.1f", cmH)} cm  ·  $dpi dpi"
        } else {
            info.text = "Tamanho inválido"
        }
        warn.text = when {
            !okSize -> "Use de 16 a 8192 pixels em cada lado."
            tooBig -> "Imagem grande demais (máximo 16 megapixels). Reduza o tamanho ou a resolução."
            else -> ""
        }
        createBtn.alpha = if (valid) 1f else 0.4f
        createBtn.isEnabled = valid
    }

    private fun create() {
        recompute()
        if (!createBtn.isEnabled) return
        var name = nameEdit.text.toString().trim()
        if (name.isEmpty()) name = ProjectStore.nextDefaultName(this)
        val paper = if (bgMode == 0) Color.WHITE else bgColor
        val p = ProjectStore.create(this, name, curW, curH, dpi, bgMode == 1, paper)
        startActivity(Intent(this, EditorActivity::class.java).putExtra("id", p.id).putExtra("new", true))
        finish()
    }
}
