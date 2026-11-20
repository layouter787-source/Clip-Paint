package com.clippaint

import android.app.Activity
import android.content.Context
import android.content.res.ColorStateList
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.PorterDuff
import android.graphics.RectF
import android.graphics.Typeface
import android.graphics.drawable.ColorDrawable
import android.graphics.drawable.Drawable
import android.graphics.drawable.GradientDrawable
import android.graphics.drawable.RippleDrawable
import android.os.Build
import android.util.TypedValue
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.view.WindowInsets
import android.widget.FrameLayout
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView

/** Paleta e medidas do tema escuro do Clip Paint. */
object T {
    val BG = Color.parseColor("#0E1014")
    val SURFACE = Color.parseColor("#181B22")
    val SURFACE2 = Color.parseColor("#222632")
    val SURFACE3 = Color.parseColor("#2C3140")
    val ACCENT = Color.parseColor("#3B82F6")
    val ACCENT_DIM = Color.parseColor("#263F6E")
    val TEXT = Color.parseColor("#EEF0F5")
    val DIM = Color.parseColor("#8B93A6")
    val DANGER = Color.parseColor("#EF4444")
    val BAR = Color.parseColor("#F2161920")  // barras flutuantes (levemente translucidas)
    val WHITE = Color.WHITE
}

fun dp(ctx: Context, v: Float): Int = (v * ctx.resources.displayMetrics.density + 0.5f).toInt()
fun View.dp(v: Float): Int = dp(context, v)
fun Context.dp(v: Float): Int = dp(this, v)

fun roundBg(color: Int, radiusPx: Float, strokeColor: Int = 0, strokePx: Int = 0): GradientDrawable {
    val g = GradientDrawable()
    g.shape = GradientDrawable.RECTANGLE
    g.cornerRadius = radiusPx
    g.setColor(color)
    if (strokePx > 0) g.setStroke(strokePx, strokeColor)
    return g
}

fun ovalBg(color: Int): GradientDrawable {
    val g = GradientDrawable()
    g.shape = GradientDrawable.OVAL
    g.setColor(color)
    return g
}

fun ripple(content: Drawable?, color: Int = 0x33FFFFFF): Drawable =
    RippleDrawable(ColorStateList.valueOf(color), content ?: ColorDrawable(Color.TRANSPARENT), null)

fun lp(w: Int, h: Int): LinearLayout.LayoutParams = LinearLayout.LayoutParams(w, h)
fun lpw(w: Int, h: Int, weight: Float): LinearLayout.LayoutParams = LinearLayout.LayoutParams(w, h, weight)
fun flp(w: Int, h: Int, gravity: Int = Gravity.NO_GRAVITY): FrameLayout.LayoutParams = FrameLayout.LayoutParams(w, h, gravity)

const val MATCH = ViewGroup.LayoutParams.MATCH_PARENT
const val WRAP = ViewGroup.LayoutParams.WRAP_CONTENT

fun label(ctx: Context, text: String, sp: Float = 14f, color: Int = T.TEXT, bold: Boolean = false): TextView {
    val t = TextView(ctx)
    t.text = text
    t.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp)
    t.setTextColor(color)
    if (bold) t.setTypeface(Typeface.DEFAULT, Typeface.BOLD)
    t.includeFontPadding = false
    return t
}

/** Botao so com icone (vetor do Material Design Icons) e tooltip. */
class IconButton(context: Context, private val res: Int, private val sizeDp: Float = 44f, private val iconDp: Float = 24f) :
    ImageView(context) {
    private var isOn = false
    private var isUsable = true

    init {
        setImageResource(res)
        scaleType = ScaleType.CENTER_INSIDE
        val pad = dp((sizeDp - iconDp) / 2f)
        setPadding(pad, pad, pad, pad)
        layoutParams = ViewGroup.LayoutParams(dp(sizeDp), dp(sizeDp))
        style(false)
    }

    fun tip(text: String): IconButton {
        tooltipText = text
        contentDescription = text
        return this
    }

    fun style(active: Boolean, usable: Boolean = true, tint: Int = T.TEXT): IconButton {
        isOn = active
        isUsable = usable
        background = if (active) ripple(roundBg(T.ACCENT, dp(sizeDp / 2.8f).toFloat()), 0x44FFFFFF)
        else ripple(roundBg(Color.TRANSPARENT, dp(sizeDp / 2.8f).toFloat()))
        setColorFilter(if (active) T.WHITE else tint, PorterDuff.Mode.SRC_IN)
        alpha = if (usable) 1f else 0.32f
        return this
    }

    fun setIcon(newRes: Int) {
        setImageResource(newRes)
    }

    fun click(action: () -> Unit): IconButton {
        setOnClickListener { if (isUsable) action() }
        return this
    }
}

/** Botao de texto arredondado (chip). */
class Chip(context: Context, text: String, private val small: Boolean = false) : TextView(context) {
    init {
        this.text = text
        setTextSize(TypedValue.COMPLEX_UNIT_SP, if (small) 12f else 13f)
        gravity = Gravity.CENTER
        includeFontPadding = false
        setPadding(dp(if (small) 10f else 14f), 0, dp(if (small) 10f else 14f), 0)
        minHeight = dp(if (small) 30f else 36f)
        setTypeface(Typeface.DEFAULT, Typeface.BOLD)
        style(false)
    }

    fun style(active: Boolean): Chip {
        background = ripple(roundBg(if (active) T.ACCENT else T.SURFACE3, dp(18f).toFloat()))
        setTextColor(if (active) T.WHITE else T.TEXT)
        return this
    }
}

/** Slider proprio: trilho fino arredondado e bolinha branca. */
class SliderView(context: Context) : View(context) {
    var value = 0f
        set(v) {
            field = v.coerceIn(0f, 1f)
            invalidate()
        }
    var onChange: ((Float) -> Unit)? = null
    var onStart: (() -> Unit)? = null
    var onEnd: (() -> Unit)? = null
    private val track = Paint(Paint.ANTI_ALIAS_FLAG)
    private val rect = RectF()
    private var dragging = false

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        setMeasuredDimension(resolveSize(dp(160f), widthMeasureSpec), dp(40f))
    }

    override fun onDraw(canvas: Canvas) {
        val pad = dp(14f).toFloat()
        val cy = height / 2f
        val th = dp(6f).toFloat()
        val l = pad
        val r = width - pad
        track.color = Color.parseColor("#33FFFFFF")
        rect.set(l, cy - th / 2, r, cy + th / 2)
        canvas.drawRoundRect(rect, th, th, track)
        track.color = T.ACCENT
        val x = l + (r - l) * value
        rect.set(l, cy - th / 2, x, cy + th / 2)
        canvas.drawRoundRect(rect, th, th, track)
        track.color = Color.parseColor("#55000000")
        canvas.drawCircle(x, cy + dp(1f), dp(12f).toFloat(), track)
        track.color = T.WHITE
        canvas.drawCircle(x, cy, dp(11f).toFloat(), track)
    }

    private fun update(ev: MotionEvent) {
        val pad = dp(14f).toFloat()
        val t = ((ev.x - pad) / (width - 2 * pad)).coerceIn(0f, 1f)
        if (t != value) {
            value = t
            onChange?.invoke(t)
        }
    }

    override fun onTouchEvent(ev: MotionEvent): Boolean {
        when (ev.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                parent?.requestDisallowInterceptTouchEvent(true)
                dragging = true
                onStart?.invoke()
                update(ev)
            }
            MotionEvent.ACTION_MOVE -> if (dragging) update(ev)
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                if (dragging) {
                    dragging = false
                    onEnd?.invoke()
                }
            }
        }
        return true
    }
}

fun divider(ctx: Context, vertical: Boolean = true): View {
    val v = View(ctx)
    v.setBackgroundColor(Color.parseColor("#22FFFFFF"))
    v.layoutParams = if (vertical) lp(dp(ctx, 1f), dp(ctx, 24f)) else lp(MATCH, dp(ctx, 1f))
    return v
}

// ------------------------------------------------------------------ janelas / insets

/** Barras do sistema transparentes e conteudo desenhado por baixo (edge-to-edge). */
fun Activity.edgeToEdge() {
    if (Build.VERSION.SDK_INT >= 30) {
        window.setDecorFitsSystemWindows(false)
    } else {
        @Suppress("DEPRECATION")
        window.decorView.systemUiVisibility = View.SYSTEM_UI_FLAG_LAYOUT_STABLE or
            View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
    }
    @Suppress("DEPRECATION")
    window.statusBarColor = Color.TRANSPARENT
    @Suppress("DEPRECATION")
    window.navigationBarColor = Color.TRANSPARENT
}

fun WindowInsets.sysTop(): Int =
    if (Build.VERSION.SDK_INT >= 30) getInsets(WindowInsets.Type.systemBars() or WindowInsets.Type.displayCutout()).top
    else @Suppress("DEPRECATION") systemWindowInsetTop

fun WindowInsets.sysBottom(): Int =
    if (Build.VERSION.SDK_INT >= 30) getInsets(WindowInsets.Type.systemBars()).bottom
    else @Suppress("DEPRECATION") systemWindowInsetBottom

fun WindowInsets.sysLeft(): Int =
    if (Build.VERSION.SDK_INT >= 30) getInsets(WindowInsets.Type.systemBars() or WindowInsets.Type.displayCutout()).left
    else @Suppress("DEPRECATION") systemWindowInsetLeft

fun WindowInsets.sysRight(): Int =
    if (Build.VERSION.SDK_INT >= 30) getInsets(WindowInsets.Type.systemBars() or WindowInsets.Type.displayCutout()).right
    else @Suppress("DEPRECATION") systemWindowInsetRight

/** Tamanho do pincel em escala logaritmica (1 a 600 px). */
object Sizes {
    fun toSize(t: Float): Float = Math.max(1f, Math.exp(t.coerceIn(0f, 1f) * Math.log(600.0)).toFloat())
    fun toSlider(d: Float): Float = (Math.log(Math.max(1f, d).toDouble()) / Math.log(600.0)).toFloat().coerceIn(0f, 1f)
}
