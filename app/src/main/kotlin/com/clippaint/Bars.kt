package com.clippaint

import android.content.Context
import android.graphics.Color
import android.view.Gravity
import android.view.View
import android.widget.HorizontalScrollView
import android.widget.LinearLayout

/** Barra flutuante com botoes de icone (usada para selecao e transformacao). */
open class FloatingBar(context: Context) : LinearLayout(context) {
    protected val scroller = HorizontalScrollView(context)
    protected val row = LinearLayout(context)

    init {
        orientation = HORIZONTAL
        background = roundBg(T.BAR, dp(30f).toFloat())
        elevation = dp(8f).toFloat()
        scroller.isHorizontalScrollBarEnabled = false
        row.orientation = HORIZONTAL
        row.gravity = Gravity.CENTER_VERTICAL
        row.setPadding(dp(6f), dp(5f), dp(6f), dp(5f))
        scroller.addView(row)
        addView(scroller, lp(WRAP, WRAP))
    }

    protected fun sep() {
        val d = divider(context)
        row.addView(d, lp(dp(1f), dp(26f)).also { it.setMargins(dp(6f), 0, dp(6f), 0) })
    }
}

/** Metodos de selecao, modos (novo/somar/tirar) e acoes. So icones; descricao no tooltip. */
class SelectionBar(context: Context, private val onChanged: () -> Unit) : FloatingBar(context) {
    private val methodBtns = ArrayList<IconButton>()
    private val opBtns = ArrayList<IconButton>()
    private val tolBox = LinearLayout(context)
    private val tolSlider = SliderView(context)
    private var hasSelection = false
    private val actionBtns = ArrayList<IconButton>()
    private val needsSelection = ArrayList<Boolean>()

    init {
        val methods = listOf(
            Triple(R.drawable.ic_lasso, "Laço (mão livre)", 0), Triple(R.drawable.ic_sel_rect, "Retângulo", 1),
            Triple(R.drawable.ic_sel_ellipse, "Elipse", 2), Triple(R.drawable.ic_wand, "Automática (por cor)", 3)
        )
        for (m in methods) {
            val b = IconButton(context, m.first, 42f, 22f).tip(m.second).click {
                Native.setSelMethod(m.third)
                refresh(hasSelection)
                onChanged()
            }
            methodBtns.add(b)
            row.addView(b, lp(dp(42f), dp(42f)))
        }
        sep()
        val ops = listOf(
            Triple(R.drawable.ic_sel_new, "Nova seleção", 0), Triple(R.drawable.ic_sel_plus, "Somar à seleção", 1),
            Triple(R.drawable.ic_sel_minus, "Tirar da seleção", 2)
        )
        for (o in ops) {
            val b = IconButton(context, o.first, 42f, 22f).tip(o.second).click {
                Native.setSelOp(o.third)
                refresh(hasSelection)
            }
            opBtns.add(b)
            row.addView(b, lp(dp(42f), dp(42f)))
        }
        sep()
        // acoes: (icone, tooltip, exige selecao, acao)
        class Act(val icon: Int, val tip: String, val needs: Boolean, val action: () -> Unit)
        val acts = listOf(
            Act(R.drawable.ic_move, "Transformar (mover, escalar, girar, perspectiva)", false) { Native.beginTransform(false) },
            Act(R.drawable.ic_duplicate, "Duplicar e transformar", true) { Native.beginTransform(true) },
            Act(R.drawable.ic_layer_add, "Copiar para nova camada", true) { Native.selAction(5) },
            Act(R.drawable.ic_select_all, "Selecionar tudo", false) { Native.selAction(0) },
            Act(R.drawable.ic_select_inverse, "Inverter seleção", false) { Native.selAction(1) },
            Act(R.drawable.ic_blur, "Suavizar bordas", true) { Native.selAction(2) },
            Act(R.drawable.ic_bucket, "Preencher com a cor", true) { Native.selAction(3) },
            Act(R.drawable.ic_clear, "Apagar conteúdo", true) { Native.selAction(4) },
            Act(R.drawable.ic_close, "Desmarcar", true) { Native.selAction(6) }
        )
        for (a in acts) {
            val b = IconButton(context, a.icon, 42f, 22f).tip(a.tip).click {
                a.action()
                onChanged()
            }
            actionBtns.add(b)
            needsSelection.add(a.needs)
            row.addView(b, lp(dp(42f), dp(42f)))
        }
        // tolerancia da selecao automatica (aparece abaixo da barra)
        tolBox.orientation = HORIZONTAL
        tolBox.gravity = Gravity.CENTER_VERTICAL
        tolBox.setPadding(dp(16f), 0, dp(12f), 0)
        tolBox.addView(IconButton(context, R.drawable.ic_tune, 28f, 18f).tip("Tolerância"), lp(dp(28f), dp(28f)))
        tolSlider.value = Native.getFill(2)
        tolSlider.onChange = { Native.setFill(2, it) }
        tolBox.addView(tolSlider, lpw(0, dp(40f), 1f))
        tolBox.visibility = View.GONE
        orientation = VERTICAL
        addView(tolBox, lp(MATCH, WRAP))
        refresh(false)
    }

    fun refresh(selection: Boolean) {
        hasSelection = selection
        val m = Native.getSelMethod()
        val o = Native.getSelOp()
        for (i in methodBtns.indices) methodBtns[i].style(i == m)
        for (i in opBtns.indices) opBtns[i].style(i == o)
        for (i in actionBtns.indices) actionBtns[i].style(false, !needsSelection[i] || selection)
        tolBox.visibility = if (m == 3) View.VISIBLE else View.GONE
    }
}

/** Barra da sessao de transformacao: modos (livre, uniforme, perspectiva), espelhar, cancelar e aplicar. */
class TransformBar(context: Context, private val onDone: () -> Unit) : FloatingBar(context) {
    private val modeBtns = ArrayList<IconButton>()

    init {
        val modes = listOf(
            Triple(R.drawable.ic_free, "Livre (estica)", 0), Triple(R.drawable.ic_uniform, "Uniforme (mantém a proporção)", 1),
            Triple(R.drawable.ic_perspective, "Perspectiva (move cada canto)", 2)
        )
        for (m in modes) {
            val b = IconButton(context, m.first, 44f, 24f).tip(m.second).click {
                Native.setTransformMode(m.third)
                refresh()
            }
            modeBtns.add(b)
            row.addView(b, lp(dp(44f), dp(44f)))
        }
        sep()
        row.addView(IconButton(context, R.drawable.ic_flip_h, 44f, 24f).tip("Espelhar na horizontal").click { Native.flipTransform(true) }, lp(dp(44f), dp(44f)))
        row.addView(IconButton(context, R.drawable.ic_flip_v, 44f, 24f).tip("Espelhar na vertical").click { Native.flipTransform(false) }, lp(dp(44f), dp(44f)))
        sep()
        row.addView(IconButton(context, R.drawable.ic_close, 44f, 24f).tip("Cancelar").style(false, true, T.DANGER).click {
            Native.cancelTransform()
            onDone()
        }, lp(dp(44f), dp(44f)))
        val ok = IconButton(context, R.drawable.ic_check, 44f, 24f).tip("Aplicar").style(true).click {
            Native.applyTransform()
            onDone()
        }
        row.addView(ok, lp(dp(44f), dp(44f)))
        refresh()
    }

    fun refresh() {
        val m = Native.getTransformMode()
        for (i in modeBtns.indices) modeBtns[i].style(i == m)
    }
}
