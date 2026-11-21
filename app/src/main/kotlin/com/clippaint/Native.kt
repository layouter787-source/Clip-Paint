package com.clippaint

import android.content.res.AssetManager
import android.view.Surface

/** Ponte JNI para o motor nativo em C++ (libnative-lib.so). */
object Native {
    init {
        System.loadLibrary("native-lib")
    }

    // ---- ciclo de vida / projeto ----
    @JvmStatic external fun initEngine(assets: AssetManager, filesDir: String, extDir: String, density: Float)
    @JvmStatic external fun openProject(path: String, w: Int, h: Int, transparent: Boolean, dpi: Int): Boolean
    @JvmStatic external fun saveProject(path: String): Boolean
    @JvmStatic external fun exportPng(path: String): Boolean
    @JvmStatic external fun saveThumbnail(path: String, maxSize: Int): Boolean
    @JvmStatic external fun surfaceCreated(surface: Surface)
    @JvmStatic external fun surfaceDestroyed()
    @JvmStatic external fun requestRender()
    @JvmStatic external fun setMargins(top: Float, bottom: Float)
    @JvmStatic external fun fitView()
    @JvmStatic external fun resetRotation()
    @JvmStatic external fun docWidth(): Int
    @JvmStatic external fun docHeight(): Int
    @JvmStatic external fun getZoom(): Float

    /** [acao, indice, nPonteiros, tempo] + por ponteiro [id, tipo, x, y, pressao, nHist] + nHist*[x, y, pressao, tempo] */
    @JvmStatic external fun touch(data: DoubleArray)

    // ---- estado (consultado por polling) ----
    /** [podeDesfazer, podeRefazer, ferramenta, transformando, temSelecao, cor, camadaAtiva, nCamadas, revisao, toastSeq] */
    @JvmStatic external fun state(): IntArray
    @JvmStatic external fun toastText(): String
    @JvmStatic external fun undo()
    @JvmStatic external fun redo()

    // ---- ferramentas e cor ----
    /** 0 pincel, 1 borracha, 2 tinta, 3 laco, 4 conta-gotas */
    @JvmStatic external fun setTool(tool: Int)
    @JvmStatic external fun getTool(): Int
    @JvmStatic external fun setColor(argb: Int)
    @JvmStatic external fun getColor(): Int
    @JvmStatic external fun addRecent(argb: Int)
    @JvmStatic external fun recents(): IntArray
    /** 0 simetria, 1 grade, 2 dedo desenha, 3 papel visivel, 4 tinta em todas as camadas */
    @JvmStatic external fun setFlag(which: Int, on: Boolean)
    @JvmStatic external fun getFlag(which: Int): Boolean
    @JvmStatic external fun setPaperColor(argb: Int)

    // ---- pinceis ----
    @JvmStatic external fun brushGroups(source: Int): Array<String>
    @JvmStatic external fun brushesIn(source: Int, group: String, eraserOnly: Boolean): IntArray
    @JvmStatic external fun brushName(index: Int): String
    @JvmStatic external fun brushGroup(index: Int): String
    @JvmStatic external fun brushDescription(index: Int): String
    /** bit0 borracha, bit1 mistura, bit2 luz, bit3 animado, bit4 AnimBrush */
    @JvmStatic external fun brushFlags(index: Int): Int
    @JvmStatic external fun brushPreview(index: Int, w: Int, h: Int): IntArray?
    @JvmStatic external fun selectBrush(eraser: Boolean, index: Int)
    @JvmStatic external fun currentBrush(eraser: Boolean): Int
    /** which: 0 diametro, 1 opacidade, 2 dureza (-1 = do pincel), 3 suavizacao */
    @JvmStatic external fun getBrushValue(eraser: Boolean, which: Int): Float
    @JvmStatic external fun setBrushValue(eraser: Boolean, which: Int, value: Float)
    /** which: 0 tolerancia da tinta, 1 expandir (px), 2 tolerancia da selecao automatica */
    @JvmStatic external fun getFill(which: Int): Float
    @JvmStatic external fun setFill(which: Int, value: Float)

    // ---- selecao ----
    /** 0 laco, 1 retangulo, 2 elipse, 3 automatica */
    @JvmStatic external fun setSelMethod(method: Int)
    @JvmStatic external fun getSelMethod(): Int
    /** 0 novo, 1 somar, 2 tirar */
    @JvmStatic external fun setSelOp(op: Int)
    @JvmStatic external fun getSelOp(): Int
    /** 0 tudo, 1 inverter, 2 suavizar, 3 pintar, 4 limpar, 5 copiar para camada, 6 desmarcar */
    @JvmStatic external fun selAction(action: Int)
    @JvmStatic external fun beginTransform(copy: Boolean): Boolean
    @JvmStatic external fun applyTransform()
    @JvmStatic external fun cancelTransform()
    /** 0 livre, 1 uniforme, 2 perspectiva */
    @JvmStatic external fun setTransformMode(mode: Int)
    @JvmStatic external fun getTransformMode(): Int
    @JvmStatic external fun flipTransform(horizontal: Boolean)

    // ---- camadas ----
    @JvmStatic external fun layerCount(): Int
    @JvmStatic external fun activeLayer(): Int
    @JvmStatic external fun layerName(index: Int): String
    /** bit0 visivel, bit1 bloqueada, bit2 bloqueio de alfa */
    @JvmStatic external fun layerFlags(index: Int): Int
    @JvmStatic external fun layerOpacity(index: Int): Float
    @JvmStatic external fun layerBlend(index: Int): String
    @JvmStatic external fun layerThumb(index: Int, w: Int, h: Int): IntArray?
    /** 0 nova, 1 duplicar, 2 apagar, 3 mesclar, 4 subir, 5 descer, 6 limpar, 7 selecionar, 8 visivel, 9 bloquear,
     *  10 bloquear alfa, 11 modo de mistura, 12 opacidade, 13 iniciar edicao de opacidade */
    @JvmStatic external fun layerOp(op: Int, index: Int, value: Float)

    // ---- timeline de animacao (frames raster associados as camadas) ----
    /** [fps, frame atual (zero-based), total de frames, reproduzindo, faixas, onion ativo, anteriores, seguintes] */
    @JvmStatic external fun animationState(): IntArray
    @JvmStatic external fun animationSeek(frame: Int): Boolean
    @JvmStatic external fun animationSetFps(fps: Int): Boolean
    @JvmStatic external fun animationSetFrameCount(count: Int): Boolean
    /** duplicate=true cria um novo frame ligado ao cel atual; false cria um cel vazio */
    @JvmStatic external fun animationInsertFrame(duplicate: Boolean): Boolean
    @JvmStatic external fun animationRemoveFrame(): Boolean
    @JvmStatic external fun animationSetPlaying(playing: Boolean): Boolean
    @JvmStatic external fun animationTrackCount(): Int
    @JvmStatic external fun animationTrackName(index: Int): String
}
