package com.clippaint

import android.app.Activity
import android.app.AlertDialog
import android.content.Intent
import android.graphics.BitmapFactory
import android.graphics.Color
import android.graphics.drawable.GradientDrawable
import android.os.Bundle
import android.text.TextUtils
import android.view.Gravity
import android.view.View
import android.view.WindowInsets
import android.widget.EditText
import android.widget.FrameLayout
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.PopupMenu
import android.widget.ScrollView
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/** Tela inicial: novo projeto + projetos recentes. */
class HomeActivity : Activity() {
    private lateinit var root: FrameLayout
    private lateinit var content: LinearLayout

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        edgeToEdge()
        root = FrameLayout(this)
        root.setBackgroundColor(T.BG)
        val scroll = ScrollView(this)
        scroll.isVerticalScrollBarEnabled = false
        content = LinearLayout(this)
        content.orientation = LinearLayout.VERTICAL
        scroll.addView(content, FrameLayout.LayoutParams(MATCH, WRAP))
        root.addView(scroll, FrameLayout.LayoutParams(MATCH, MATCH))
        setContentView(root)
        root.setOnApplyWindowInsetsListener { _, insets ->
            content.setPadding(
                dp(20f) + insets.sysLeft(), dp(16f) + insets.sysTop(),
                dp(20f) + insets.sysRight(), dp(32f) + insets.sysBottom()
            )
            insets
        }
        root.requestApplyInsets()
    }

    override fun onResume() {
        super.onResume()
        refresh()
    }

    private fun refresh() {
        content.removeAllViews()
        content.addView(header())
        content.addView(heroCard())
        val projects = ProjectStore.list(this)
        val title = label(this, "Projetos recentes", 18f, T.TEXT, true)
        val tp = lp(MATCH, WRAP)
        tp.topMargin = dp(28f)
        tp.bottomMargin = dp(12f)
        content.addView(title, tp)
        if (projects.isEmpty()) {
            content.addView(emptyState())
        } else {
            val cols = if (resources.configuration.screenWidthDp >= 720) 3 else 2
            var i = 0
            while (i < projects.size) {
                val row = LinearLayout(this)
                row.orientation = LinearLayout.HORIZONTAL
                for (c in 0 until cols) {
                    val cell = FrameLayout(this)
                    val p = lpw(0, WRAP, 1f)
                    p.rightMargin = if (c < cols - 1) dp(12f) else 0
                    p.bottomMargin = dp(14f)
                    if (i + c < projects.size) cell.addView(projectCard(projects[i + c]))
                    row.addView(cell, p)
                }
                content.addView(row, lp(MATCH, WRAP))
                i += cols
            }
        }
    }

    private fun header(): View {
        val row = LinearLayout(this)
        row.orientation = LinearLayout.HORIZONTAL
        row.gravity = Gravity.CENTER_VERTICAL
        val logo = FrameLayout(this)
        logo.background = roundBg(T.ACCENT, dp(14f).toFloat())
        val ic = ImageView(this)
        ic.setImageResource(R.drawable.ic_brush)
        ic.setColorFilter(Color.WHITE)
        logo.addView(ic, flp(dp(26f), dp(26f), Gravity.CENTER))
        row.addView(logo, lp(dp(46f), dp(46f)))
        val col = LinearLayout(this)
        col.orientation = LinearLayout.VERTICAL
        col.addView(label(this, "Clip Paint", 26f, T.TEXT, true))
        val sub = label(this, "Estúdio de desenho", 13f, T.DIM)
        col.addView(sub, lp(WRAP, WRAP).also { it.topMargin = dp(3f) })
        val cp = lp(0, WRAP)
        cp.weight = 1f
        cp.leftMargin = dp(14f)
        row.addView(col, cp)
        return row
    }

    private fun heroCard(): View {
        val card = LinearLayout(this)
        card.orientation = LinearLayout.HORIZONTAL
        card.gravity = Gravity.CENTER_VERTICAL
        val g = GradientDrawable(GradientDrawable.Orientation.TL_BR, intArrayOf(Color.parseColor("#3B82F6"), Color.parseColor("#7C3AED")))
        g.cornerRadius = dp(28f).toFloat()
        card.background = ripple(g, 0x33FFFFFF)
        card.elevation = dp(6f).toFloat()
        card.setPadding(dp(22f), dp(22f), dp(22f), dp(22f))
        val circle = FrameLayout(this)
        circle.background = ovalBg(Color.parseColor("#33FFFFFF"))
        val ic = ImageView(this)
        ic.setImageResource(R.drawable.ic_plus)
        ic.setColorFilter(Color.WHITE)
        circle.addView(ic, flp(dp(30f), dp(30f), Gravity.CENTER))
        card.addView(circle, lp(dp(56f), dp(56f)))
        val col = LinearLayout(this)
        col.orientation = LinearLayout.VERTICAL
        col.addView(label(this, "Novo projeto", 20f, Color.WHITE, true))
        col.addView(label(this, "Nome, tamanho e resolução", 13f, Color.parseColor("#CCFFFFFF")), lp(WRAP, WRAP).also { it.topMargin = dp(4f) })
        val cp = lp(0, WRAP)
        cp.weight = 1f
        cp.leftMargin = dp(16f)
        card.addView(col, cp)
        val go = ImageView(this)
        go.setImageResource(R.drawable.ic_back)
        go.rotation = 180f
        go.setColorFilter(Color.WHITE)
        card.addView(go, lp(dp(24f), dp(24f)))
        card.setOnClickListener { startActivity(Intent(this, NewProjectActivity::class.java)) }
        val lpp = lp(MATCH, WRAP)
        lpp.topMargin = dp(24f)
        card.layoutParams = lpp
        return card
    }

    private fun emptyState(): View {
        val box = LinearLayout(this)
        box.orientation = LinearLayout.VERTICAL
        box.gravity = Gravity.CENTER
        box.background = roundBg(T.SURFACE, dp(24f).toFloat())
        box.setPadding(dp(20f), dp(40f), dp(20f), dp(40f))
        val ic = ImageView(this)
        ic.setImageResource(R.drawable.ic_gallery)
        ic.setColorFilter(T.DIM)
        box.addView(ic, lp(dp(44f), dp(44f)))
        val t = label(this, "Nenhum projeto ainda", 15f, T.TEXT, true)
        box.addView(t, lp(WRAP, WRAP).also { it.topMargin = dp(12f) })
        val s = label(this, "Toque em “Novo projeto” para começar a desenhar.", 13f, T.DIM)
        s.gravity = Gravity.CENTER
        box.addView(s, lp(WRAP, WRAP).also { it.topMargin = dp(6f) })
        return box
    }

    private fun projectCard(p: Project): View {
        val card = LinearLayout(this)
        card.orientation = LinearLayout.VERTICAL
        card.background = ripple(roundBg(T.SURFACE, dp(22f).toFloat()))
        card.setPadding(dp(10f), dp(10f), dp(10f), dp(12f))

        val thumbBox = FrameLayout(this)
        thumbBox.background = roundBg(T.SURFACE3, dp(14f).toFloat())
        val img = ImageView(this)
        img.scaleType = ImageView.ScaleType.FIT_CENTER
        val tf = p.thumb(this)
        if (tf.exists()) {
            try {
                img.setImageBitmap(BitmapFactory.decodeFile(tf.path))
            } catch (e: Exception) {
                img.setImageResource(R.drawable.ic_image)
            }
            img.setPadding(dp(8f), dp(8f), dp(8f), dp(8f))
        } else {
            img.setImageResource(R.drawable.ic_image)
            img.setColorFilter(T.DIM)
            img.setPadding(dp(40f), dp(40f), dp(40f), dp(40f))
        }
        thumbBox.addView(img, FrameLayout.LayoutParams(MATCH, MATCH))
        val more = IconButton(this, R.drawable.ic_more, 36f, 20f).tip("Mais opções")
        more.background = ripple(ovalBg(Color.parseColor("#CC0E1014")))
        more.setOnClickListener { showMenu(more, p) }
        thumbBox.addView(more, flp(dp(36f), dp(36f), Gravity.TOP or Gravity.END).also { it.setMargins(0, dp(6f), dp(6f), 0) })
        card.addView(thumbBox, lp(MATCH, dp(150f)))

        val name = label(this, p.name, 14f, T.TEXT, true)
        name.maxLines = 1
        name.ellipsize = TextUtils.TruncateAt.END
        card.addView(name, lp(MATCH, WRAP).also { it.topMargin = dp(10f) })
        val info = label(this, "${p.w}×${p.h} px · ${p.dpi} dpi", 12f, T.DIM)
        card.addView(info, lp(MATCH, WRAP).also { it.topMargin = dp(4f) })
        val date = SimpleDateFormat("dd/MM/yyyy HH:mm", Locale.getDefault()).format(Date(p.modified))
        card.addView(label(this, date, 11f, T.DIM), lp(MATCH, WRAP).also { it.topMargin = dp(2f) })

        card.setOnClickListener {
            startActivity(Intent(this, EditorActivity::class.java).putExtra("id", p.id))
        }
        return card
    }

    private fun showMenu(anchor: View, p: Project) {
        val m = PopupMenu(this, anchor)
        m.menu.add(0, 1, 0, "Renomear")
        m.menu.add(0, 2, 1, "Duplicar")
        m.menu.add(0, 3, 2, "Excluir")
        m.setOnMenuItemClickListener { item ->
            when (item.itemId) {
                1 -> rename(p)
                2 -> {
                    ProjectStore.duplicate(this, p)
                    refresh()
                }
                3 -> confirmDelete(p)
            }
            true
        }
        m.show()
    }

    private fun rename(p: Project) {
        val input = EditText(this)
        input.setText(p.name)
        input.setSelectAllOnFocus(true)
        input.setTextColor(T.TEXT)
        AlertDialog.Builder(this, android.R.style.Theme_Material_Dialog_Alert)
            .setTitle("Renomear projeto")
            .setView(input)
            .setPositiveButton("Salvar") { _, _ ->
                val n = input.text.toString().trim()
                if (n.isNotEmpty()) {
                    p.name = n
                    ProjectStore.save(this, p)
                    refresh()
                }
            }
            .setNegativeButton("Cancelar", null)
            .show()
    }

    private fun confirmDelete(p: Project) {
        AlertDialog.Builder(this, android.R.style.Theme_Material_Dialog_Alert)
            .setTitle("Excluir projeto?")
            .setMessage("“${p.name}” será removido permanentemente.")
            .setPositiveButton("Excluir") { _, _ ->
                ProjectStore.delete(this, p)
                refresh()
            }
            .setNegativeButton("Cancelar", null)
            .show()
    }
}
