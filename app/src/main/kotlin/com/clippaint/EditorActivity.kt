package com.clippaint

import android.app.Activity
import android.content.Context
import android.graphics.Color
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.view.View
import android.view.WindowInsets
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.PopupMenu
import android.widget.TextView
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/** Editor: o canvas e desenhado pelo motor nativo; barras e paineis sao Views Android. */
class EditorActivity : Activity() {
    companion object {
        private var nativeReady = false
        fun ensureNative(ctx: Context) {
            if (nativeReady) return
            Native.initEngine(ctx.assets, ctx.filesDir.absolutePath, ctx.getExternalFilesDir(null)?.absolutePath ?: "", ctx.resources.displayMetrics.density)
            nativeReady = true
        }
    }

    private lateinit var project: Project
    private lateinit var root: FrameLayout
    private lateinit var canvas: CanvasView
    private lateinit var topLeft: LinearLayout
    private lateinit var topRight: LinearLayout
    private lateinit var bottomRow: LinearLayout
    private lateinit var selBar: SelectionBar
    private lateinit var trBar: TransformBar
    private lateinit var panelHost: FrameLayout
    private lateinit var toolPanel: ToolPanel
    private lateinit var layersPanel: LayersPanel
    private lateinit var animationPanel: AnimationPanel
    private lateinit var toastView: TextView
    private lateinit var undoBtn: IconButton
    private lateinit var redoBtn: IconButton
    private lateinit var swatch: View
    private lateinit var eyeBtn: IconButton
    private val toolBtns = ArrayList<IconButton>()
    private val handler = Handler(Looper.getMainLooper())
    private var insets = intArrayOf(0, 0, 0, 0)  // topo, base, esquerda, direita
    private var lastToast = -1
    private var lastState = IntArray(0)
    private var ticks = 0
    private var fitted = false
    private var panelKind = 0  // 0 nenhum, 1 ferramenta, 2 camadas, 3 animacao

    private val ticker = object : Runnable {
        override fun run() {
            tick()
            handler.postDelayed(this, 100)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        edgeToEdge()
        ensureNative(this)
        val id = intent.getStringExtra("id")
        val p = if (id != null) ProjectStore.load(this, id) else null
        if (p == null) {
            finish()
            return
        }
        project = p
        buildUi()
        Native.openProject(project.cpd(this).absolutePath, project.w, project.h, project.transparent, project.dpi)
        if (intent.getBooleanExtra("new", false) && !project.transparent) Native.setPaperColor(project.paper)
    }

    // ------------------------------------------------------------------ construcao

    private fun pill(vararg views: View): LinearLayout {
        val l = LinearLayout(this)
        l.orientation = LinearLayout.HORIZONTAL
        l.gravity = Gravity.CENTER_VERTICAL
        l.background = roundBg(T.BAR, dp(26f).toFloat())
        l.elevation = dp(8f).toFloat()
        l.setPadding(dp(3f), dp(3f), dp(3f), dp(3f))
        for (v in views) l.addView(v)
        return l
    }

    private fun buildUi() {
        root = FrameLayout(this)
        root.setBackgroundColor(Color.parseColor("#2B2D33"))
        canvas = CanvasView(this)
        canvas.onTouchStart = { closePanels() }
        root.addView(canvas, FrameLayout.LayoutParams(MATCH, MATCH))

        // barra superior
        val home = IconButton(this, R.drawable.ic_back, 46f, 24f).tip("Início").click { finish() }
        val layers = IconButton(this, R.drawable.ic_layers, 46f, 24f).tip("Camadas").click { toggleLayers() }
        val animation = label(this, "Anim", 12f, T.TEXT, true).apply {
            setPadding(dp(12f), dp(10f), dp(12f), dp(10f))
            background = roundBg(T.SURFACE2, dp(18f).toFloat())
            contentDescription = "Abrir timeline de animação"
            setOnClickListener { toggleAnimation() }
        }
        topLeft = pill(home, layers, animation)
        undoBtn = IconButton(this, R.drawable.ic_undo, 46f, 24f).tip("Desfazer").click { Native.undo() }
        redoBtn = IconButton(this, R.drawable.ic_redo, 46f, 24f).tip("Refazer").click { Native.redo() }
        val more = IconButton(this, R.drawable.ic_more, 46f, 24f).tip("Mais opções")
        more.setOnClickListener { showMenu(more) }
        topRight = pill(undoBtn, redoBtn, more)
        root.addView(topLeft, flp(WRAP, WRAP, Gravity.TOP or Gravity.START))
        root.addView(topRight, flp(WRAP, WRAP, Gravity.TOP or Gravity.END))

        selBar = SelectionBar(this) { tick() }
        trBar = TransformBar(this) { tick() }
        root.addView(selBar, flp(WRAP, WRAP, Gravity.TOP or Gravity.CENTER_HORIZONTAL))
        root.addView(trBar, flp(WRAP, WRAP, Gravity.TOP or Gravity.CENTER_HORIZONTAL))
        selBar.visibility = View.GONE
        trBar.visibility = View.GONE

        // paineis
        panelHost = FrameLayout(this)
        toolPanel = ToolPanel(this, { closePanels() }, { startEyedropper() })
        layersPanel = LayersPanel(this) { closePanels() }
        animationPanel = AnimationPanel(this) { tick() }
        panelHost.addView(toolPanel, FrameLayout.LayoutParams(MATCH, MATCH))
        panelHost.addView(layersPanel, FrameLayout.LayoutParams(MATCH, MATCH))
        panelHost.addView(animationPanel, FrameLayout.LayoutParams(MATCH, MATCH))
        toolPanel.visibility = View.GONE
        layersPanel.visibility = View.GONE
        animationPanel.visibility = View.GONE
        panelHost.visibility = View.GONE
        root.addView(panelHost, flp(MATCH, dp(480f), Gravity.BOTTOM))

        // barra de ferramentas inferior: so icones
        swatch = View(this)
        swatch.setOnClickListener { openColor() }
        val swatchWrap = FrameLayout(this)
        swatchWrap.background = roundBg(T.BAR, dp(28f).toFloat())
        swatchWrap.elevation = dp(8f).toFloat()
        swatchWrap.addView(swatch, flp(dp(30f), dp(30f), Gravity.CENTER))

        val tools = listOf(
            Pair(R.drawable.ic_brush, "Pincel"), Pair(R.drawable.ic_eraser, "Borracha"),
            Pair(R.drawable.ic_bucket, "Tinta"), Pair(R.drawable.ic_lasso, "Seleção")
        )
        val pillTools = LinearLayout(this)
        pillTools.orientation = LinearLayout.HORIZONTAL
        pillTools.gravity = Gravity.CENTER_VERTICAL
        pillTools.background = roundBg(T.BAR, dp(34f).toFloat())
        pillTools.elevation = dp(10f).toFloat()
        pillTools.setPadding(dp(6f), dp(6f), dp(6f), dp(6f))
        for (i in tools.indices) {
            val b = IconButton(this, tools[i].first, 56f, 28f).tip(tools[i].second)
            b.setOnClickListener { onToolTap(i) }
            toolBtns.add(b)
            pillTools.addView(b, lp(dp(56f), dp(56f)).also { it.setMargins(dp(2f), 0, dp(2f), 0) })
        }
        eyeBtn = IconButton(this, R.drawable.ic_eyedropper, 48f, 26f).tip("Conta-gotas")
        eyeBtn.setOnClickListener { startEyedropper() }
        val eyeWrap = FrameLayout(this)
        eyeWrap.background = roundBg(T.BAR, dp(28f).toFloat())
        eyeWrap.elevation = dp(8f).toFloat()
        eyeWrap.addView(eyeBtn, flp(dp(48f), dp(48f), Gravity.CENTER))

        bottomRow = LinearLayout(this)
        bottomRow.orientation = LinearLayout.HORIZONTAL
        bottomRow.gravity = Gravity.CENTER_VERTICAL
        bottomRow.addView(swatchWrap, lp(dp(56f), dp(56f)))
        bottomRow.addView(pillTools, lp(WRAP, dp(68f)).also { it.setMargins(dp(10f), 0, dp(10f), 0) })
        bottomRow.addView(eyeWrap, lp(dp(56f), dp(56f)))
        root.addView(bottomRow, flp(WRAP, WRAP, Gravity.BOTTOM or Gravity.CENTER_HORIZONTAL))

        toastView = label(this, "", 14f, Color.WHITE, true)
        toastView.setPadding(dp(18f), dp(11f), dp(18f), dp(11f))
        toastView.background = roundBg(Color.parseColor("#F0101218"), dp(22f).toFloat())
        toastView.elevation = dp(12f).toFloat()
        toastView.visibility = View.GONE
        root.addView(toastView, flp(WRAP, WRAP, Gravity.BOTTOM or Gravity.CENTER_HORIZONTAL))

        setContentView(root)
        root.setOnApplyWindowInsetsListener { _, wi ->
            insets = intArrayOf(wi.sysTop(), wi.sysBottom(), wi.sysLeft(), wi.sysRight())
            applyLayout()
            wi
        }
        root.addOnLayoutChangeListener { _, _, _, _, _, _, _, _, _ -> applyLayout() }
        root.requestApplyInsets()
    }

    private fun applyLayout() {
        val w = root.width
        val h = root.height
        if (w == 0 || h == 0) return
        val (t, b, l, r) = insets.toList()
        fun margins(v: View, ml: Int, mt: Int, mr: Int, mb: Int) {
            val lpp = v.layoutParams as FrameLayout.LayoutParams
            if (lpp.leftMargin != ml || lpp.topMargin != mt || lpp.rightMargin != mr || lpp.bottomMargin != mb) {
                lpp.setMargins(ml, mt, mr, mb)
                v.layoutParams = lpp
            }
        }
        val edge = dp(12f)
        margins(topLeft, l + edge, t + dp(10f), 0, 0)
        margins(topRight, 0, t + dp(10f), r + edge, 0)
        val barTop = t + dp(10f) + dp(58f) + dp(4f)
        margins(selBar, edge + l, barTop, edge + r, 0)
        margins(trBar, edge + l, barTop, edge + r, 0)
        margins(bottomRow, 0, 0, 0, b + dp(12f))
        margins(toastView, 0, 0, 0, b + dp(12f) + dp(68f) + dp(14f))

        val portrait = w < h
        val panelLp = panelHost.layoutParams as FrameLayout.LayoutParams
        val bottomSpace = b + dp(12f) + dp(68f) + dp(10f)
        val ph = Math.min(dp(560f), (h * 0.62f).toInt())
        val pw = if (portrait) w - 2 * edge - l - r else Math.min(dp(430f), (w * 0.46f).toInt())
        val gravity = if (portrait) Gravity.BOTTOM or Gravity.CENTER_HORIZONTAL else Gravity.BOTTOM or Gravity.END
        if (panelLp.height != ph || panelLp.width != pw || panelLp.bottomMargin != bottomSpace || panelLp.gravity != gravity ||
            panelLp.rightMargin != (if (portrait) 0 else r + edge)
        ) {
            panelLp.height = ph
            panelLp.width = pw
            panelLp.gravity = gravity
            panelLp.setMargins(0, 0, if (portrait) 0 else r + edge, bottomSpace)
            panelHost.layoutParams = panelLp
        }
        // margens do canvas (para "ajustar a tela")
        val top = (t + dp(10f) + dp(58f)).toFloat()
        val bottom = (b + dp(12f) + dp(68f) + dp(10f)).toFloat()
        Native.setMargins(top, bottom)
        if (!fitted) {
            fitted = true
            Native.fitView()
        }
    }

    // ------------------------------------------------------------------ interacao

    private fun onToolTap(i: Int) {
        val cur = Native.getTool()
        if (cur == i && i <= 2) {
            if (panelKind == 1) closePanels() else openToolPanel(i, if (i == 2) 1 else 0)
        } else {
            Native.setTool(i)
            closePanels()
        }
        tick()
    }

    private fun openToolPanel(tool: Int, tab: Int) {
        panelKind = 1
        layersPanel.visibility = View.GONE
        animationPanel.visibility = View.GONE
        toolPanel.visibility = View.VISIBLE
        panelHost.visibility = View.VISIBLE
        toolPanel.open(tool, tab)
    }

    private fun openColor() {
        var t = Native.getTool()
        if (t > 2) {
            Native.setTool(0)
            t = 0
        }
        openToolPanel(t, 2)
    }

    private fun toggleLayers() {
        if (panelKind == 2) {
            closePanels()
            return
        }
        panelKind = 2
        toolPanel.visibility = View.GONE
        animationPanel.visibility = View.GONE
        layersPanel.visibility = View.VISIBLE
        panelHost.visibility = View.VISIBLE
        layersPanel.refresh()
    }

    private fun closePanels() {
        panelKind = 0
        panelHost.visibility = View.GONE
        toolPanel.visibility = View.GONE
        layersPanel.visibility = View.GONE
        animationPanel.visibility = View.GONE
    }

    private fun toggleAnimation() {
        if (panelKind == 3) {
            closePanels()
            return
        }
        panelKind = 3
        toolPanel.visibility = View.GONE
        layersPanel.visibility = View.GONE
        animationPanel.visibility = View.VISIBLE
        panelHost.visibility = View.VISIBLE
        animationPanel.refresh(true)
    }

    private fun startEyedropper() {
        Native.setTool(4)
        closePanels()
        showToast("Toque e arraste na tela para escolher uma cor")
    }

    private fun showMenu(anchor: View) {
        val m = PopupMenu(this, anchor)
        m.menu.add(0, 1, 0, "Salvar")
        m.menu.add(0, 2, 1, "Exportar PNG")
        m.menu.add(0, 3, 2, "Ajustar à tela")
        m.menu.add(0, 4, 3, "Endireitar rotação")
        m.menu.add(0, 5, 4, "Simetria").isCheckable = true
        m.menu.findItem(5).isChecked = Native.getFlag(0)
        m.menu.add(0, 6, 5, "Grade").isCheckable = true
        m.menu.findItem(6).isChecked = Native.getFlag(1)
        m.menu.add(0, 7, 6, "Dedo desenha").isCheckable = true
        m.menu.findItem(7).isChecked = Native.getFlag(2)
        m.menu.add(0, 8, 7, "Fundo visível").isCheckable = true
        m.menu.findItem(8).isChecked = Native.getFlag(3)
        m.menu.add(0, 9, 8, "Fundo = cor atual")
        m.menu.add(0, 10, 9, "Animação / Timeline")
        m.setOnMenuItemClickListener { item ->
            when (item.itemId) {
                1 -> {
                    saveNow()
                    showToast("Projeto salvo")
                }
                2 -> exportPng()
                3 -> Native.fitView()
                4 -> Native.resetRotation()
                5 -> Native.setFlag(0, !Native.getFlag(0))
                6 -> Native.setFlag(1, !Native.getFlag(1))
                7 -> Native.setFlag(2, !Native.getFlag(2))
                8 -> Native.setFlag(3, !Native.getFlag(3))
                9 -> Native.setPaperColor(Native.getColor())
                10 -> toggleAnimation()
            }
            true
        }
        m.show()
    }

    private fun exportPng() {
        val dir = getExternalFilesDir("exports") ?: File(filesDir, "exports")
        dir.mkdirs()
        val name = "ClipPaint_" + SimpleDateFormat("yyyyMMdd_HHmmss", Locale.US).format(Date()) + ".png"
        val ok = Native.exportPng(File(dir, name).absolutePath)
        showToast(if (ok) "PNG salvo em exports/$name" else "Falha ao exportar")
    }

    private fun saveNow() {
        if (!::project.isInitialized) return
        Native.saveProject(project.cpd(this).absolutePath)
        Native.saveThumbnail(project.thumb(this).absolutePath, 512)
        project.modified = System.currentTimeMillis()
        ProjectStore.save(this, project)
    }

    private fun showToast(msg: String) {
        if (msg.isEmpty()) return
        toastView.text = msg
        toastView.visibility = View.VISIBLE
        handler.removeCallbacks(hideToast)
        handler.postDelayed(hideToast, 2200)
    }

    private val hideToast = Runnable { toastView.visibility = View.GONE }

    // ------------------------------------------------------------------ estado

    private fun tick() {
        val s = Native.state()
        if (s.size < 10) return
        ticks++
        if (!s.contentEquals(lastState)) {
            val first = lastState.isEmpty()
            val old = lastState
            lastState = s
            undoBtn.style(false, s[0] == 1)
            redoBtn.style(false, s[1] == 1)
            val tool = s[2]
            for (i in toolBtns.indices) toolBtns[i].style(tool == i)
            eyeBtn.style(tool == 4)
            swatch.background = ovalBg(s[5])
            val transforming = s[3] == 1
            val selecting = tool == 3 && !transforming
            selBar.visibility = if (selecting) View.VISIBLE else View.GONE
            trBar.visibility = if (transforming) View.VISIBLE else View.GONE
            if (selecting) selBar.refresh(s[4] == 1)
            if (transforming) trBar.refresh()
            if (panelKind == 1 && tool > 2) closePanels()
            if (s[9] != lastToast) {
                lastToast = s[9]
                if (!first) showToast(Native.toastText())
            }
            if (panelKind == 2 && (first || old.size < 10 || s[8] != old[8] || s[6] != old[6])) layersPanel.refresh()
        }
        if (panelKind == 2 && ticks % 10 == 0) layersPanel.refresh()
        if (panelKind == 1) toolPanel.tick()
        if (panelKind == 3) animationPanel.refresh()
    }

    // ------------------------------------------------------------------ ciclo de vida

    override fun onResume() {
        super.onResume()
        if (!::project.isInitialized) return
        handler.post(ticker)
    }

    override fun onPause() {
        handler.removeCallbacks(ticker)
        if (::animationPanel.isInitialized) Native.animationSetPlaying(false)
        saveNow()
        super.onPause()
    }

    @Deprecated("usa o botao voltar do sistema")
    override fun onBackPressed() {
        if (panelKind != 0) {
            closePanels()
            return
        }
        @Suppress("DEPRECATION")
        super.onBackPressed()
    }
}
