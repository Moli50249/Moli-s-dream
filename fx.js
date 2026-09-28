/* fx.js —— 由 src/web_fx.cpp 生成，请改那里 */
window.FX = (function () {
  "use strict";

  var raf = window.requestAnimationFrame
    ? window.requestAnimationFrame.bind(window)
    : function (fn) { return setTimeout(function () { fn(Date.now()); }, 16); };
  var caf = window.cancelAnimationFrame
    ? window.cancelAnimationFrame.bind(window)
    : clearTimeout;

  function clamp(v, a, b) { return v < a ? a : (v > b ? b : v); }
  function rand(a, b) { return a + Math.random() * (b - a); }

  /* ---------------- 取色：颜色都在 CSS 里 ---------------- */

  var C = {};
  function cssVar(name, fb) {
    var v = getComputedStyle(document.documentElement).getPropertyValue(name);
    v = (v || "").trim();
    return v || fb;
  }
  function hex2rgb(h) {
    h = (h || "").trim();
    if (h.charAt(0) === "#") h = h.slice(1);
    if (h.length === 3) h = h[0] + h[0] + h[1] + h[1] + h[2] + h[2];
    var n = parseInt(h, 16);
    if (isNaN(n)) return [255, 255, 255];
    return [(n >> 16) & 255, (n >> 8) & 255, n & 255];
  }
  function mix(a, b, k) {
    return [Math.round(a[0] + (b[0] - a[0]) * k),
            Math.round(a[1] + (b[1] - a[1]) * k),
            Math.round(a[2] + (b[2] - a[2]) * k)];
  }

  var dirty = true;
  function retheme() {
    C = {
      fx: [cssVar("--fx-a", "#d8f8ff"), cssVar("--fx-b", "#7fdcff"), cssVar("--fx-c", "#2e9bff"),
           cssVar("--fx-d", "#12309a"), cssVar("--fx-e", "#060f2c")].map(hex2rgb),
      cursor: [cssVar("--cur-a", "#9ef7ff"), cssVar("--cur-b", "#ff2e8b"), cssVar("--cur-c", "#ffd166")].map(hex2rgb),
      chipBg: cssVar("--chip-bg", "#050b1c"),
      chipLine: hx(cssVar("--chip-line", "#1c3f8f")),
      chipPin: hx(cssVar("--chip-pin", "#3566d8")),
      chipGlow: hx(cssVar("--chip-glow", "#8fe3ff"))
    };
    dirty = true;
  }
  // "r,g,b" 形式的字符串，方便直接拼 rgba()
  function hx(h) {
    var c = hex2rgb(h);
    return c[0] + "," + c[1] + "," + c[2];
  }
  function rgba(rgb, a) {
    return "rgba(" + rgb[0] + "," + rgb[1] + "," + rgb[2] + "," + a + ")";
  }

  /* ---------------- 画布尺寸 ---------------- */

  function fit(cv) {
    var dpr = clamp(window.devicePixelRatio || 1, 1, 2);
    var r = cv.getBoundingClientRect();
    var w = Math.max(1, Math.round(r.width));
    var h = Math.max(1, Math.round(r.height));
    cv.width = Math.round(w * dpr);
    cv.height = Math.round(h * dpr);
    var ctx = cv.getContext("2d");
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.imageSmoothingEnabled = false;
    return { w: w, h: h, ctx: ctx, dpr: dpr };
  }

  /* ============================================================
     1. 像素火焰：从底部升起的几个大块 + 往上飘的小粒子
     不做细节模拟，就是几块大方块从画面底部往上飘：
       · 越往上颜色越暗（底部接近白青，顶部化进深蓝背景），也越透明
       · 横向只做整格的左右轻摆，保证每一格都是硬边方块
       · 另外撒一些更小更快的亮点往上窜，像火星
     ============================================================ */

  var fire = (function () {
    var cv = null, ctx = null;
    var W = 0, H = 0, cell = 18, cols = 0, rows = 0;
    var ramp = null;                        // 量化好的颜色档，块按高度取色
    var blocks = [], sparks = [];
    var running = false, last = 0, t = 0, accB = 0, accS = 0;

    // 5 档色标插值出 24 级色阶：k=0 是底部最亮，k=1 是顶部最暗
    function buildRamp() {
      var stops = C.fx || [];
      var N = 24;
      ramp = [];
      for (var i = 0; i < N; i++) {
        var k = i / (N - 1);
        var p = k * (stops.length - 1);
        var a = Math.floor(p), b = Math.min(a + 1, stops.length - 1);
        var c = mix(stops[a], stops[b], p - a);
        // 越往上越透明，免得在页面上半部糊成一片
        ramp.push(rgba(c, Math.max(0, 1 - 0.78 * k).toFixed(3)));
      }
    }

    function layout() {
      var d = fit(cv);
      W = d.w; H = d.h; ctx = d.ctx;
      // 格子给得很大：整屏只有几十列，火焰就是"几个大块"而不是细碎火苗
      cell = clamp(Math.round(W / 76), 14, 26);
      cols = Math.max(10, Math.ceil(W / cell));
      rows = Math.max(10, Math.ceil(H / cell));
      blocks = [];
      sparks = [];
    }

    function wantBlocks() { return clamp(Math.round(cols / 3), 8, 34); }

    function spawnBlock(fromBottom) {
      blocks.push({
        x: (Math.random() * cols) | 0,
        y: fromBottom ? rows + rand(0, 1.5) : rand(2, rows),   // 单位：格
        vy: rand(1.1, 2.6),                                   // 格/秒
        w: Math.random() < 0.34 ? 2 : 1,                      // 少数是两格宽的大块
        h: Math.random() < 0.28 ? 2 : 1,
        ph: Math.random() * 6.283,
        amp: rand(0, 0.8)
      });
    }

    function spawnSpark() {
      sparks.push({
        x: Math.random() * cols,
        y: rows + rand(0, 2),
        vx: rand(-0.5, 0.5),
        vy: rand(2.6, 6.0),
        age: 0,
        life: rand(2.0, 4.0),
        s: Math.random() < 0.35 ? 6 : 4                        // 小方块的边长（像素）
      });
    }

    function step(dt) {
      t += dt;

      // 数量按屏宽来：宽屏多几块，手机上少几块
      accB += dt;
      if (blocks.length < wantBlocks() && accB > 0.07) { accB = 0; spawnBlock(true); }
      accS += dt;
      if (sparks.length < 110 && accS > 0.03) { accS = 0; spawnSpark(); }

      ctx.clearRect(0, 0, W, H);

      // 大块：往上飘，越飘越暗。升到大约 3/4 屏高就彻底暗掉，
      // 所以火始终"贴着下边烧"，不会跑到页面顶部去
      for (var i = blocks.length - 1; i >= 0; i--) {
        var b = blocks[i];
        b.y -= b.vy * dt;
        var k = clamp((1 - b.y / rows) / 0.75, 0, 1);
        if (k >= 1 || b.y < -2) { blocks.splice(i, 1); continue; }
        var xi = b.x + Math.round(Math.sin(t * 1.4 + b.ph) * b.amp);
        // 升到一半就收掉一格：像火苗往上收细，也不会一直是同样大的方块
        var shrink = k > 0.6 ? 1 : 0;
        var bw = Math.max(1, b.w - shrink) * cell;
        var bh = Math.max(1, b.h - shrink) * cell;
        ctx.fillStyle = ramp[clamp(Math.round(k * (ramp.length - 1)), 0, ramp.length - 1)];
        ctx.fillRect(xi * cell, Math.round(b.y) * cell, bw, bh);
      }

      // 小粒子：更快更小，闪一下再淡出
      var stops = C.fx || [];
      for (var j = sparks.length - 1; j >= 0; j--) {
        var s = sparks[j];
        s.age += dt;
        if (s.age >= s.life) { sparks.splice(j, 1); continue; }
        s.y -= s.vy * dt;
        s.x += s.vx * dt;
        if (Math.random() < 0.06) continue;                  // 随机漏画一帧 = 闪烁
        var kk = s.age / s.life;
        ctx.fillStyle = rgba(stops[kk < 0.6 ? 0 : 1], ((1 - kk) * 0.95).toFixed(3));
        ctx.fillRect(Math.round(s.x * cell), Math.round(s.y * cell), s.s, s.s);
      }
    }

    function loop(now) {
      if (!running) return;
      raf(loop);
      if (document.hidden) return;
      if (!last) last = now;
      // 30fps 足够，省电
      if (now - last < 30) return;
      var dt = Math.min((now - last) / 1000, 0.05);
      last = now;
      if (dirty) { buildRamp(); dirty = false; }
      step(dt);
    }

    function start(canvas, opts) {
      cv = canvas;
      if (!cv || !cv.getContext) return;
      layout();
      buildRamp();
      dirty = false;
      window.addEventListener("resize", function () {
        if (!running) return;
        layout();
        buildRamp();
      });
      // 动效关闭时画一帧静态的就好，不要一直跑
      if (opts && opts.reduced) {
        for (var i = 0; i < wantBlocks(); i++) spawnBlock(false);
        for (var j = 0; j < 40; j++) spawnSpark();
        step(0);
        return;
      }
      running = true;
      raf(loop);
    }
    function stop() { running = false; }

    return { start: start, stop: stop };
  })();

  /* ============================================================
     2. 光标粒子：跟着鼠标走的像素碎屑
     鼠标移动时按距离撒点，粒子带一点惯性、轻微重力，用叠加混合发光；
     按下时炸开一圈。指针离开窗口就自然淡完。
     ============================================================ */

  var cursor = (function () {
    var cv = null, ctx = null, W = 0, H = 0;
    var ps = [], running = false, last = 0;
    var mx = -999, my = -999, tx = -999, ty = -999, px = -999, py = -999;
    var seen = false;

    function spawn(x, y, vx, vy, big) {
      var col = C.cursor[(Math.random() * C.cursor.length) | 0];
      ps.push({
        x: x, y: y, vx: vx, vy: vy,
        life: rand(0.5, 1.05), age: 0,
        size: big ? rand(3, 6) : rand(2, 4),
        col: col
      });
      if (ps.length > 260) ps.shift();
    }

    function loop(now) {
      if (!running) return;
      raf(loop);
      if (document.hidden) return;
      if (!last) last = now;
      var dt = Math.min((now - last) / 1000, 0.05);
      last = now;

      // 光标位置用补间追，指针停下时还会滑一小段
      if (seen) {
        tx += (mx - tx) * 0.35;
        ty += (my - ty) * 0.35;
      }

      ctx.clearRect(0, 0, W, H);
      ctx.globalCompositeOperation = "lighter";

      for (var i = ps.length - 1; i >= 0; i--) {
        var p = ps[i];
        p.age += dt;
        if (p.age >= p.life) { ps.splice(i, 1); continue; }
        p.vy += 22 * dt;
        p.vx *= 0.985;
        p.vy *= 0.985;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        var k = 1 - p.age / p.life;
        var s = Math.max(1, Math.round(p.size * (0.35 + k * 0.65)));
        ctx.globalAlpha = clamp(k * 0.9, 0, 1);
        ctx.fillStyle = rgba(p.col, 1);
        // 方块像素，跟整站的像素风统一
        ctx.fillRect(Math.round(p.x), Math.round(p.y), s, s);
      }

      // 光标本体：一个小小的像素十字
      if (seen) {
        ctx.globalAlpha = 0.55;
        ctx.fillStyle = rgba(C.cursor[0], 1);
        var cx = Math.round(tx), cy = Math.round(ty);
        ctx.fillRect(cx - 7, cy, 5, 2);
        ctx.fillRect(cx + 3, cy, 5, 2);
        ctx.fillRect(cx, cy - 7, 2, 5);
        ctx.fillRect(cx, cy + 3, 2, 5);
      }

      ctx.globalAlpha = 1;
      ctx.globalCompositeOperation = "source-over";
    }

    function start(canvas) {
      cv = canvas;
      if (!cv || !cv.getContext) return;
      var d = fit(cv);
      W = d.w; H = d.h; ctx = d.ctx;
      running = true;
      window.addEventListener("resize", function () {
        if (!running) return;
        var r = fit(cv);
        W = r.w; H = r.h; ctx = r.ctx;
      });
      window.addEventListener("pointermove", function (e) {
        var x = e.clientX, y = e.clientY;
        if (!seen) { seen = true; tx = mx = x; ty = my = y; }
        var dx = x - mx, dy = y - my;
        mx = x; my = y;
        if (px > -900) {
          var dist = Math.abs(x - px) + Math.abs(y - py);
          if (dist > 5) {
            px = x; py = y;
            var n = Math.min(3, 1 + (dist / 26 | 0));
            for (var i = 0; i < n; i++) {
              spawn(x + rand(-3, 3), y + rand(-3, 3), -dx * 2.2 + rand(-14, 14), -dy * 2.2 + rand(-16, 6), false);
            }
          }
        } else { px = x; py = y; }
      }, { passive: true });
      window.addEventListener("pointerdown", function (e) {
        for (var i = 0; i < 14; i++) {
          var a = Math.random() * Math.PI * 2, sp = rand(60, 210);
          spawn(e.clientX, e.clientY, Math.cos(a) * sp, Math.sin(a) * sp, true);
        }
      }, { passive: true });
      window.addEventListener("pointerleave", function () { seen = false; });
      window.addEventListener("blur", function () { seen = false; });
      raf(loop);
    }
    function stop() { running = false; }

    return { start: start, stop: stop };
  })();

  /* ============================================================
     3. 芯片电流：四周走线向中心芯片汇聚
     中间一颗方芯片（四边有脚位、内部是裸片网格），四条边上各引出若干条
     折线走线（曼哈顿走线，带过孔），脉冲沿着走线往芯片里涌，到达脚位时
     脚位和裸片闪一下 —— 就是图 3 那种"四周电流往中间涌"的 2D 效果。
     ============================================================ */

  var chip = (function () {
    var cv = null, ctx = null, W = 0, H = 0;
    var traces = [], flashes = [], running = false, last = 0, t0 = 0;
    var opts = { label: "", sub: "" };
    var box = null;

    function layout() {
      var d = fit(cv);
      W = d.w; H = d.h; ctx = d.ctx;

      var side = clamp(Math.min(W * 0.3, H * 0.46), 96, 210);
      var cx = W * 0.62, cy = H * 0.5;
      box = { x: cx - side / 2, y: cy - side / 2, s: side, cx: cx, cy: cy };

      traces = [];
      flashes = [];
      var n = clamp(Math.round(W / 78), 10, 22);
      var per = Math.ceil(n / 4);

      for (var e = 0; e < 4; e++) {
        for (var i = 0; i < per; i++) {
          var k = (i + 1) / (per + 1);
          var pts = [];

          if (e === 0) {            // 从左边进来
            pts.push({ x: 0, y: H * k });
            pts.push({ x: box.x - side * 0.5, y: H * k });
            pts.push({ x: box.x - side * 0.5, y: pinY(k) });
            pts.push({ x: box.x - 3, y: pinY(k) });
          } else if (e === 1) {     // 从右边进来
            pts.push({ x: W, y: H * k });
            pts.push({ x: box.x + side + side * 0.5, y: H * k });
            pts.push({ x: box.x + side + side * 0.5, y: pinY(k) });
            pts.push({ x: box.x + side + 3, y: pinY(k) });
          } else if (e === 2) {     // 从上面下来
            pts.push({ x: W * k, y: 0 });
            pts.push({ x: W * k, y: box.y - side * 0.45 });
            pts.push({ x: pinX(k), y: box.y - side * 0.45 });
            pts.push({ x: pinX(k), y: box.y - 3 });
          } else {                  // 从下面上去
            pts.push({ x: W * k, y: H });
            pts.push({ x: W * k, y: box.y + side + side * 0.45 });
            pts.push({ x: pinX(k), y: box.y + side + side * 0.45 });
            pts.push({ x: pinX(k), y: box.y + side + 3 });
          }

          // 折线总长，脉冲按长度匀速走
          var len = 0;
          for (var s = 1; s < pts.length; s++) {
            len += Math.abs(pts[s].x - pts[s - 1].x) + Math.abs(pts[s].y - pts[s - 1].y);
          }
          traces.push({
            pts: pts, len: len,
            pos: Math.random(),
            speed: rand(0.24, 0.5),
            edge: e,
            flash: 0
          });
        }
      }
    }

    function pinY(k) { return box.y + box.s * (0.16 + k * 0.68); }
    function pinX(k) { return box.x + box.s * (0.16 + k * 0.68); }

    function pointAt(tr, t) {
      var want = tr.len * t;
      var acc = 0;
      for (var i = 1; i < tr.pts.length; i++) {
        var a = tr.pts[i - 1], b = tr.pts[i];
        var seg = Math.abs(b.x - a.x) + Math.abs(b.y - a.y);
        if (acc + seg >= want) {
          var k = seg > 0 ? (want - acc) / seg : 0;
          return { x: a.x + (b.x - a.x) * k, y: a.y + (b.y - a.y) * k };
        }
        acc += seg;
      }
      var lastP = tr.pts[tr.pts.length - 1];
      return { x: lastP.x, y: lastP.y };
    }

    function drawTrace(tr) {
      ctx.beginPath();
      ctx.moveTo(tr.pts[0].x, tr.pts[0].y);
      for (var i = 1; i < tr.pts.length; i++) ctx.lineTo(tr.pts[i].x, tr.pts[i].y);
      ctx.strokeStyle = "rgba(" + C.chipLine + ",.55)";
      ctx.lineWidth = 1;
      ctx.stroke();

      // 过孔：走线拐点上点几个小方块
      ctx.fillStyle = "rgba(" + C.chipLine + ",.75)";
      for (var j = 1; j < tr.pts.length - 1; j++) {
        ctx.fillRect(Math.round(tr.pts[j].x) - 2, Math.round(tr.pts[j].y) - 2, 4, 4);
      }
    }

    function drawChip() {
      var x = box.x, y = box.y, s = box.s;

      // 脚位
      ctx.fillStyle = "rgba(" + C.chipPin + ",.9)";
      var legs = 9;
      for (var i = 0; i < legs; i++) {
        var k = (i + 0.5) / legs;
        var w = 3, l = 8;
        ctx.fillRect(Math.round(x - l), Math.round(y + s * k - w / 2), l, w);
        ctx.fillRect(Math.round(x + s), Math.round(y + s * k - w / 2), l, w);
        ctx.fillRect(Math.round(x + s * k - w / 2), Math.round(y - l), w, l);
        ctx.fillRect(Math.round(x + s * k - w / 2), Math.round(y + s), w, l);
      }

      // 封装本体
      var pulse = 0.5 + 0.5 * Math.sin((performance.now() - t0) / 900);
      ctx.fillStyle = "rgba(6,12,26,.96)";
      ctx.fillRect(x, y, s, s);
      ctx.strokeStyle = "rgba(" + C.chipPin + "," + (0.55 + pulse * 0.35) + ")";
      ctx.lineWidth = 2;
      ctx.strokeRect(x + 1, y + 1, s - 2, s - 2);

      // 裸片：网格点阵（图 3 里芯片中心那一块方格）
      var pad = s * 0.16;
      var dx = x + pad, dy = y + pad, ds = s - pad * 2;
      ctx.fillStyle = "rgba(" + C.chipGlow + ",.14)";
      ctx.fillRect(dx, dy, ds, ds);
      var cells = 7;
      var cell = ds / cells;
      for (var r = 0; r < cells; r++) {
        for (var c = 0; c < cells; c++) {
          var on = ((r * 5 + c * 3) % 4) === 0;
          var lit = flashes.length && ((r + c) % 3 === 0);
          ctx.fillStyle = on || lit
            ? "rgba(" + C.chipGlow + "," + (lit ? 0.5 : 0.3) + ")"
            : "rgba(" + C.chipLine + ",.5)";
          ctx.fillRect(Math.round(dx + c * cell + 1), Math.round(dy + r * cell + 1),
                       Math.ceil(cell - 2), Math.ceil(cell - 2));
        }
      }

      // 丝印
      ctx.fillStyle = "rgba(" + C.chipGlow + ",.9)";
      ctx.font = "600 " + Math.max(10, Math.round(s * 0.075)) + "px ui-monospace, Consolas, monospace";
      ctx.textAlign = "center";
      ctx.textBaseline = "middle";
      if (opts.label) ctx.fillText(opts.label, box.cx, box.cy - s * 0.06);
      ctx.fillStyle = "rgba(" + C.chipLine + ",.9)";
      ctx.font = Math.round(Math.max(8, s * 0.055)) + "px ui-monospace, Consolas, monospace";
      if (opts.sub) ctx.fillText(opts.sub, box.cx, box.cy + s * 0.07);
    }

    function loop(now) {
      if (!running) return;
      raf(loop);
      if (document.hidden) return;
      if (!last) last = now;
      var dt = Math.min((now - last) / 1000, 0.05);
      last = now;
      if (!t0) t0 = now;

      ctx.clearRect(0, 0, W, H);

      // 底噪网格
      ctx.fillStyle = "rgba(" + C.chipLine + ",.10)";
      for (var gx = 0; gx < W; gx += 22) {
        for (var gy = 0; gy < H; gy += 22) ctx.fillRect(gx, gy, 1, 1);
      }

      for (var i = 0; i < traces.length; i++) drawTrace(traces[i]);
      drawChip();

      // 脉冲
      ctx.globalCompositeOperation = "lighter";
      for (var j = 0; j < traces.length; j++) {
        var tr = traces[j];
        tr.pos += tr.speed * dt;
        if (tr.pos >= 1) {
          tr.pos = 0;
          tr.flash = 1;
          flashes.push({ x: tr.pts[tr.pts.length - 1].x, y: tr.pts[tr.pts.length - 1].y, age: 0 });
          if (flashes.length > 24) flashes.shift();
        }
        var p = pointAt(tr, tr.pos);
        var size = 4;
        ctx.fillStyle = "rgba(" + C.chipGlow + ",.95)";
        ctx.fillRect(Math.round(p.x) - size / 2, Math.round(p.y) - size / 2, size, size);
        ctx.fillStyle = "rgba(" + C.chipGlow + ",.22)";
        ctx.fillRect(Math.round(p.x) - size * 1.8, Math.round(p.y) - size * 1.8, size * 3.6, size * 3.6);

        // 入口处再点一颗，暗示"电从画面外灌进来"
        var e0 = tr.pts[0];
        ctx.fillStyle = "rgba(" + C.chipGlow + ",.5)";
        ctx.fillRect(Math.round(e0.x) - 1, Math.round(e0.y) - 1, 3, 3);
      }

      for (var k = flashes.length - 1; k >= 0; k--) {
        var f = flashes[k];
        f.age += dt;
        if (f.age > 0.5) { flashes.splice(k, 1); continue; }
        var kk = 1 - f.age / 0.5;
        ctx.fillStyle = "rgba(" + C.chipGlow + "," + (kk * 0.8) + ")";
        var r = 8 + (1 - kk) * 14;
        ctx.fillRect(Math.round(f.x - r / 2), Math.round(f.y - r / 2), r, r);
      }
      ctx.globalCompositeOperation = "source-over";
    }

    function start(canvas, o) {
      cv = canvas;
      if (!cv || !cv.getContext) return;
      opts = o || opts;
      layout();
      running = true;
      window.addEventListener("resize", layout);
      raf(loop);
    }
    function stop() { running = false; }

    return { start: start, stop: stop };
  })();

  /* ============================================================
     4. 像素图：C++ 里用字符网格定义的吉祥物
     字符 → 颜色（见 src/main.cpp 里的 kMascot）：
       # 黑线   . 透明（背景纸面，C++ 生成时已经漫水判定过）
       C 深青   c 浅青   w 白（图案内部的白，比如颅腔和牙齿）
       （下面这些是给别的图留的通用色：s 皮肤 h 头发 p 眼睛 m 嘴 k 深色 b 蓝色）
     每格放大 scale 倍画成方块；带上下浮动、偶尔眨眼、青蓝部分呼吸。
     ============================================================ */

  var sprite = (function () {
    var PAL = {
      "o": "#141821",
      "#": "#1b1b1f",
      "e": "#ffffff",
      "p": "#243f8f",
      "m": "#d0716b",
      "s": "#f6d7c0",
      "h": "#2a4a92",
      "H": "#4d7ad2",
      "w": "#ffffff",
      "k": "#232a3a",
      "b": "#2b62c8",
      "B": "#e8ecf2",
      "d": "#7f868f",
      "C": "#11befd",
      "c": "#8ce1fe"
    };

    function paint(cv, art, animated) {
      if (!cv || !cv.getContext || !art || !art.rows) return;
      var rows = art.rows;
      var cols = 0;
      rows.forEach(function (r) { cols = Math.max(cols, r.length); });
      var scale = art.scale || 4;
      cv.width = cols * scale;
      // 底下多留一行：待机动画会让整只小人上下跳一格，不留缝就会被裁掉
      cv.height = rows.length * scale + scale;
      var ctx = cv.getContext("2d");
      ctx.imageSmoothingEnabled = false;

      function draw(offsetY, blink, pulse) {
        ctx.clearRect(0, 0, cv.width, cv.height);
        for (var y = 0; y < rows.length; y++) {
          var line = rows[y];
          for (var x = 0; x < line.length; x++) {
            var ch = line.charAt(x);
            if (ch === "." || ch === " ") continue;   // 背景：什么都不画
            var col = PAL[ch] || PAL["k"];
            // 眨眼：把眼睛涂成皮肤色，看起来就是闭眼了
            if (blink && (ch === "e" || ch === "p")) col = PAL["s"];
            // 青蓝的部件（头发/衣服）轻轻呼吸
            ctx.globalAlpha = (ch === "C" || ch === "c") ? (0.76 + 0.24 * pulse) : 1;
            ctx.fillStyle = col;
            ctx.fillRect(x * scale, Math.round(y * scale + offsetY), scale, scale);
          }
        }
        ctx.globalAlpha = 1;
      }

      if (!animated) { draw(0, false); return; }

      var t = 0, lastBlink = 0;
      (function loop() {
        if (!cv.isConnected) return;
        raf(loop);
        if (document.hidden) return;
        t += 1 / 60;
        // 跳一格（正好一个像素格的高度），而不是半格：像素画要整格动才不糊
        var bob = Math.sin(t * 3.1) > 0.45 ? scale : 0;
        var blow = Math.sin(t * 0.42) > 0.985;
        var pulse = 0.5 + 0.5 * Math.sin(t * 2.2);
        draw(bob, blow, pulse);
      })();
    }

    return { paint: paint };
  })();

  /* ============================================================
     5. 赛博像素 WELCOME
     把文字画在一张很小的离屏画布上（每个字母才十几像素高），再关掉插值
     整数倍放大贴到显示画布上 —— 得到的是实打实的方块像素，不依赖像素字体。
     配色：深空底 + 品红通道分离 + 扫描线 + 逐字母点亮的节奏。
     ============================================================ */

  var WELCOME_CYAN = "#9EF7FF";
  var WELCOME_GLOW = "#017A80";
  var WELCOME_MAGENTA = "#FF2E8B";
  var FONT = '"Cascadia Mono", "Consolas", "Courier New", monospace';

  function welcome(host, word, done, forced) {
    var cv = host.querySelector("#welcome-canvas");
    var logEl = host.querySelector(".w-log");
    var barEl = host.querySelector(".w-bar i");

    if (!cv || !cv.getContext) { done(); return; }
    var ctx = cv.getContext("2d");

    var cssW = clamp(window.innerWidth * 0.8, 260, 760);
    var cssH = cssW * 0.29;

    var off = document.createElement("canvas");
    var octx = off.getContext("2d");

    var s, lo, hi, lowW, letters = [];

    function build() {
      var dpr = clamp(window.devicePixelRatio || 1, 1, 2);
      cv.width = Math.round(cssW * dpr);
      cv.height = Math.round(cssH * dpr);
      cv.style.width = cssW + "px";
      cv.style.height = cssH + "px";
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      ctx.imageSmoothingEnabled = false;

      ctx.font = "700 100px " + FONT;
      var natural = ctx.measureText(word).width || 1;
      s = clamp(Math.floor((cssW * 0.9) / natural * 100 / 8), 2, 8);

      lo = clamp(Math.round(cssW / s), 32, 240);
      hi = Math.round(cssH / s);
      off.width = lo;
      off.height = hi;
      octx.imageSmoothingEnabled = false;

      var fpx = Math.floor(hi * 0.66);
      octx.font = "700 " + fpx + "px " + FONT;
      lowW = octx.measureText(word).width;

      letters = [];
      for (var i = 0; i < word.length; i++) {
        letters.push({ ch: word.charAt(i), x: (lowW / word.length) * i, on: false });
      }
    }

    build();
    window.addEventListener("resize", build);

    var lines = ["BOOT SEQUENCE .......... OK", "LOADING MODULES ........ OK",
                 "MOUNTING /home/guest ... OK", "READY."];
    var full = lines.join("\n");

    var T_LETTER = 140, T_TYPE = 1050, T_GLITCH = 520, T_TOTAL = 2080;

    letters.forEach(function (L, i) {
      setTimeout(function () { L.on = true; }, i * T_LETTER);
    });

    var li = 0;
    var typeTimer = setInterval(function () {
      li++;
      if (logEl) logEl.textContent = full.slice(0, li);
      if (li >= full.length) clearInterval(typeTimer);
    }, T_TYPE / full.length);

    if (barEl) {
      barEl.style.transition = "width " + (T_TOTAL - 260) + "ms cubic-bezier(.3,.9,.3,1)";
      setTimeout(function () { barEl.style.width = "100%"; }, 60);
    }

    host.classList.add("on");

    var t0 = performance.now();

    function draw(now) {
      var t = now - t0;

      function rise(L, i) {
        if (!L.on) return 1;
        var local = t - i * T_LETTER;
        if (local > 420) return 0;
        var k = clamp(local / 420, 0, 1);
        return (1 - k) * (1 - k) * 3.2;
      }

      var g = 0;
      if (t > 260) {
        var growth = clamp((t - 260) / (T_GLITCH - 260), 0.15, 1);
        var fade = t > T_GLITCH ? clamp(1 - (t - T_GLITCH) / (T_TOTAL - T_GLITCH), 0, 1) : 1;
        g = growth * fade;
      }

      octx.clearRect(0, 0, lo, hi);
      octx.textBaseline = "middle";

      var baseX = (lo - lowW) / 2;
      var baseY = hi * 0.5;

      for (var i = 0; i < letters.length; i++) {
        var L = letters[i];
        if (!L.on) continue;
        var gx = g * 2, gy = g * -1.6, amp = (1 + g * 1.5);

        octx.fillStyle = WELCOME_MAGENTA;
        octx.globalAlpha = 0.42 * amp;
        octx.fillText(L.ch, baseX + L.x - gx, baseY + rise(L, i) + gy);

        octx.fillStyle = WELCOME_CYAN;
        octx.globalAlpha = 1;
        octx.fillText(L.ch, baseX + L.x, baseY + rise(L, i));

        if (g > 0.35 && ((i * 7 + Math.floor(t / 48)) % 6 === 0)) {
          octx.globalAlpha = 0.85;
          octx.fillText(L.ch, baseX + L.x + gx * 2.2, baseY + rise(L, i));
        }
        octx.globalAlpha = 1;
      }

      ctx.clearRect(0, 0, cssW, cssH);
      ctx.imageSmoothingEnabled = false;
      ctx.drawImage(off, 0, 0, lo, hi, 0, 0, lo * s, hi * s);

      var scanY = (t / T_TOTAL) * (hi * s + 120) - 60;
      ctx.globalAlpha = 0.22;
      ctx.fillStyle = WELCOME_CYAN;
      for (var k = 0; k < 5; k++) ctx.fillRect(0, scanY + k * 4, lo * s, 2);
      ctx.globalAlpha = 1;

      var segW = 8, segGap = 4;
      var total = Math.floor((lo * s) / (segW + segGap));
      var lit = Math.floor(clamp(t / T_TOTAL, 0, 1) * total);
      var sx0 = (lo * s - total * (segW + segGap)) / 2;
      var sy = hi * s + 16;
      for (var q = 0; q < total; q++) {
        ctx.fillStyle = q < lit ? WELCOME_CYAN : "rgba(158, 247, 255, .16)";
        ctx.fillRect(sx0 + q * (segW + segGap), sy, segW, 6);
      }

      if (t < T_TOTAL) raf(draw);
      else finishOut();
    }

    function finishOut() {
      clearInterval(typeTimer);
      if (logEl) logEl.textContent = full;
      host.classList.add("out");
      setTimeout(function () {
        host.classList.remove("on", "out");
        if (barEl) barEl.style.width = "0";
        if (logEl) logEl.textContent = "";
        done();
      }, 460);
    }

    raf(draw);
  }

  retheme();

  return {
    retheme: retheme,
    fire: fire,
    cursor: cursor,
    chip: chip,
    sprite: sprite,
    welcome: welcome
  };
})();
