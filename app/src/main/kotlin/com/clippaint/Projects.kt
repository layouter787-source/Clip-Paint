package com.clippaint

import android.content.Context
import org.json.JSONObject
import java.io.File

/** Metadados de um projeto (a imagem fica em project.cpd; a miniatura em thumb.png). */
class Project(
    val id: String,
    var name: String,
    val w: Int,
    val h: Int,
    val dpi: Int,
    val transparent: Boolean,
    val paper: Int,
    val created: Long,
    var modified: Long
) {
    fun dir(ctx: Context): File = File(ProjectStore.root(ctx), id)
    fun cpd(ctx: Context): File = File(dir(ctx), "project.cpd")
    fun thumb(ctx: Context): File = File(dir(ctx), "thumb.png")
}

object ProjectStore {
    fun root(ctx: Context): File {
        val f = File(ctx.filesDir, "projects")
        if (!f.exists()) f.mkdirs()
        return f
    }

    private fun metaFile(ctx: Context, id: String) = File(File(root(ctx), id), "meta.json")

    fun list(ctx: Context): List<Project> {
        val out = ArrayList<Project>()
        val dirs = root(ctx).listFiles() ?: return out
        for (d in dirs) {
            if (!d.isDirectory) continue
            val p = load(ctx, d.name)
            if (p != null) out.add(p)
        }
        out.sortByDescending { it.modified }
        return out
    }

    fun load(ctx: Context, id: String): Project? {
        val f = metaFile(ctx, id)
        if (!f.exists()) return null
        return try {
            val j = JSONObject(f.readText())
            Project(
                id, j.optString("name", "Sem título"), j.optInt("w", 2048), j.optInt("h", 1536),
                j.optInt("dpi", 300), j.optBoolean("transparent", false), j.optInt("paper", -1),
                j.optLong("created", 0L), j.optLong("modified", 0L)
            )
        } catch (e: Exception) {
            null
        }
    }

    fun save(ctx: Context, p: Project) {
        val j = JSONObject()
        j.put("name", p.name)
        j.put("w", p.w)
        j.put("h", p.h)
        j.put("dpi", p.dpi)
        j.put("transparent", p.transparent)
        j.put("paper", p.paper)
        j.put("created", p.created)
        j.put("modified", p.modified)
        val d = p.dir(ctx)
        if (!d.exists()) d.mkdirs()
        metaFile(ctx, p.id).writeText(j.toString())
    }

    fun create(ctx: Context, name: String, w: Int, h: Int, dpi: Int, transparent: Boolean, paper: Int): Project {
        val now = System.currentTimeMillis()
        val p = Project(now.toString(), name, w, h, dpi, transparent, paper, now, now)
        save(ctx, p)
        return p
    }

    fun delete(ctx: Context, p: Project) {
        p.dir(ctx).deleteRecursively()
    }

    fun duplicate(ctx: Context, p: Project): Project {
        val now = System.currentTimeMillis()
        val copy = Project((now + 1).toString(), p.name + " cópia", p.w, p.h, p.dpi, p.transparent, p.paper, now, now)
        save(ctx, copy)
        try {
            if (p.cpd(ctx).exists()) p.cpd(ctx).copyTo(copy.cpd(ctx), overwrite = true)
            if (p.thumb(ctx).exists()) p.thumb(ctx).copyTo(copy.thumb(ctx), overwrite = true)
        } catch (e: Exception) {
            // copia parcial: o projeto novo abre em branco
        }
        return copy
    }

    fun nextDefaultName(ctx: Context): String {
        val names = list(ctx).map { it.name }.toSet()
        var i = 1
        while (names.contains("Sem título $i")) i++
        return "Sem título $i"
    }
}
