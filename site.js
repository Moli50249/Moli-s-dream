/* site.js —— 由 src/web_script.cpp 生成，请改那里 */
(function () {
  "use strict";

  var root = document.documentElement;
  var body = document.body;

  /* ---------------- 基础工具 ---------------- */

  var raf = window.requestAnimationFrame
    ? window.requestAnimationFrame.bind(window)
    : function (fn) { return setTimeout(function () { fn(Date.now()); }, 16); };
  var caf = window.cancelAnimationFrame
    ? window.cancelAnimationFrame.bind(window)
    : clearTimeout;

  function clamp(v, a, b) { return v < a ? a : (v > b ? b : v); }
  function lerp(a, b, k) { return a + (b - a) * k; }

  var ease = {
    out: function (t) { return 1 - Math.pow(1 - t, 3); },
    outSoft: function (t) { return 1 - Math.pow(1 - t, 2.2); },
    inOut: function (t) { return t < 0.5 ? 4 * t * t * t : 1 - Math.pow(-2 * t + 2, 3) / 2; },
    spring: function (t) {
      // 轻微过冲再收回，像弹簧落位
      return 1 + 2.4 * Math.pow(t - 1, 3) + 1.6 * Math.pow(t - 1, 2);
    }
  };

  /* 逐帧补间：dur 毫秒、delay 毫秒，step(进度, 原始进度) 每帧回调。
     返回一个 cancel 函数。全程不用 CSS transition，所以不受系统动效设置影响。 */
  function tween(dur, delay, fn, step, done) {
    var t0 = -1, id = 0, dead = false;
    function frame(now) {
      if (dead) return;
      if (t0 < 0) t0 = now;
      var el = now - t0 - (delay || 0);
      if (el < 0) { id = raf(frame); return; }
      var t = dur > 0 ? clamp(el / dur, 0, 1) : 1;
      step(fn(t), t);
      if (t < 1) id = raf(frame);
      else if (done) done();
    }
    id = raf(frame);
    return function () { dead = true; caf(id); };
  }

  var motionAlways = !root.classList.contains("motion-auto");
  var systemReduce = window.matchMedia
    ? window.matchMedia("(prefers-reduced-motion: reduce)").matches
    : false;
  // 只有显式配置成 motion = auto 时才听系统设置
  var reduced = motionAlways ? false : systemReduce;

  // 降低动效时直接用"立即到位"，避免留下半透明的中间态
  function applyNow(fn) { fn(1); }

  /* ---------------- 主题 ---------------- */

  function initTheme() {
    var saved = null;
    try { saved = localStorage.getItem("theme"); } catch (e) { }
    var sysLight = window.matchMedia
      ? window.matchMedia("(prefers-color-scheme: light)").matches
      : false;
    var theme = saved || (sysLight ? "light" : "dark");
    root.setAttribute("data-theme", theme);

    var icon = document.getElementById("theme-icon");
    if (icon) icon.textContent = theme === "dark" ? "◐" : "◑";

    var btn = document.getElementById("theme-btn");
    if (!btn) return;
    btn.addEventListener("click", function () {
      theme = root.getAttribute("data-theme") === "dark" ? "light" : "dark";
      root.setAttribute("data-theme", theme);
      if (icon) icon.textContent = theme === "dark" ? "◐" : "◑";
      try { localStorage.setItem("theme", theme); } catch (e) { }
      // 画布的颜色是从 CSS 变量里读的，换主题要重新取一次
      if (window.FX && FX.retheme) FX.retheme();
    });
  }

  /* ---------------- 窄屏抽屉 ---------------- */

  function initDrawer() {
    var btn = document.getElementById("nav-toggle");
    var scrim = document.getElementById("nav-scrim");
    if (!btn || !scrim) return;

    function open(on) {
      body.classList.toggle("nav-open", on);
      btn.setAttribute("aria-expanded", on ? "true" : "false");
      if (on) {
        scrim.hidden = false;
        raf(function () { scrim.classList.add("on"); });
      } else {
        scrim.classList.remove("on");
        setTimeout(function () { if (!body.classList.contains("nav-open")) scrim.hidden = true; }, 320);
      }
    }

    btn.addEventListener("click", function () { open(!body.classList.contains("nav-open")); });
    scrim.addEventListener("click", function () { open(false); });
    document.addEventListener("keydown", function (e) {
      if (e.key === "Escape" && body.classList.contains("nav-open")) open(false);
    });
    open.close = function () { open(false); };
    window.__closeDrawer = open.close;
  }

  /* ---------------- 导航：当前区块 + 滑块 + 进度 ---------------- */

  var sections = [];
  var links = [];

  function initNav(spy) {
    sections = spy;
    links = Array.prototype.slice.call(document.querySelectorAll(".side-link"));

    var pill = document.getElementById("side-pill");
    var nav = document.getElementById("side-nav");
    var railFill = document.getElementById("side-progress");
    var topFill = document.querySelector("#progress i");

    var pillY = 0, pillH = 38, targetY = 0, targetH = 38, pillReady = false;
    var railK = 0, railTarget = 0;
    var activeId = "";
    var topK = 0, topTarget = 0;

    function activeFromLinks() {
      for (var i = 0; i < links.length; i++) {
        if (links[i].classList.contains("active")) return links[i];
      }
      return null;
    }

    function measure(a, snap) {
      if (!a || !pill) return;
      targetY = a.offsetTop;
      targetH = a.offsetHeight;
      if (!pillReady || snap) {
        pillReady = true;
        pillY = targetY; pillH = targetH;
        pill.style.transform = "translateY(" + pillY + "px)";
        pill.style.height = pillH + "px";
      }
    }

    function setActive(id, snap) {
      if (id === activeId) return;
      activeId = id;
      for (var i = 0; i < links.length; i++) {
        var on = links[i].getAttribute("data-target") === id;
        links[i].classList.toggle("active", on);
        if (on) measure(links[i], snap);
      }
    }

    // 当前区块：取视口中间那一条水平线上的区块
    function pick() {
      var mid = window.innerHeight * 0.42;
      var best = null, bestD = 1e9;
      for (var i = 0; i < sections.length; i++) {
        var r = sections[i].getBoundingClientRect();
        if (r.bottom < 0 || r.top > window.innerHeight) continue;
        var d = Math.abs(r.top - mid);
        if (r.top <= mid && r.bottom >= mid) { best = sections[i]; bestD = 0; break; }
        if (d < bestD) { bestD = d; best = sections[i]; }
      }
      if (best) setActive(best.id, false);
    }

    var first = activeFromLinks();
    if (first) setActive(first.getAttribute("data-target"), true);

    function frame() {
      raf(frame);

      // 滑块：位置与高度都补间过去，切换区块时是"滑"过去而不是跳
      if (pill) {
        var k = reduced ? 1 : 0.17;
        pillY = lerp(pillY, targetY, k);
        pillH = lerp(pillH, targetH, k);
        if (Math.abs(pillY - targetY) < 0.2) pillY = targetY;
        if (Math.abs(pillH - targetH) < 0.2) pillH = targetH;
        pill.style.transform = "translateY(" + pillY.toFixed(2) + "px)";
        pill.style.height = pillH.toFixed(2) + "px";
      }

      // 侧栏竖向进度 + 窄屏顶部进度
      railK = lerp(railK, railTarget, reduced ? 1 : 0.16);
      topK = lerp(topK, topTarget, reduced ? 1 : 0.16);
      if (railFill) railFill.style.transform = "scaleY(" + railK.toFixed(4) + ")";
      if (topFill) topFill.style.transform = "scaleX(" + topK.toFixed(4) + ")";

      pick();
    }
    raf(frame);

    function onScroll() {
      var h = document.documentElement.scrollHeight - window.innerHeight;
      var p = h > 0 ? clamp((window.scrollY || window.pageYOffset || 0) / h, 0, 1) : 0;
      railTarget = p;
      topTarget = p;
    }
    window.addEventListener("scroll", onScroll, { passive: true });
    window.addEventListener("resize", function () { measure(activeFromLinks(), true); onScroll(); });
    onScroll();
    if (nav) nav.addEventListener("click", function () { if (window.__closeDrawer) window.__closeDrawer(); });
  }

  /* ---------------- 快速查找 ---------------- */

  function initSearch() {
    var input = document.getElementById("side-q");
    var box = document.getElementById("side-hits");
    var el = document.getElementById("site-index");
    if (!input || !box || !el) return;

    var index = [];
    try { index = JSON.parse(el.textContent || "[]") || []; } catch (e) { index = []; }
    if (!index.length) return;

    var cursor = -1;
    var shown = [];

    function hit(kind, sectionTitle, sectionId, label, sub) {
      return { kind: kind, s: sectionId, st: sectionTitle, label: label, sub: sub || sectionTitle };
    }

    function match(q) {
      var lq = q.toLowerCase();
      var out = [];
      for (var i = 0; i < index.length && out.length < 24; i++) {
        var it = index[i];
        var hay = (it.label + " " + (it.kind || "") + " " + (it.t || "")).toLowerCase();
        if (hay.indexOf(lq) >= 0) out.push(it);
      }
      return out;
    }

    function mark(text, q) {
      var i = text.toLowerCase().indexOf(q.toLowerCase());
      if (i < 0) return document.createTextNode(text);
      var f = document.createDocumentFragment();
      f.appendChild(document.createTextNode(text.slice(0, i)));
      var m = document.createElement("mark");
      m.textContent = text.slice(i, i + q.length);
      f.appendChild(m);
      f.appendChild(document.createTextNode(text.slice(i + q.length)));
      return f;
    }

    function close() {
      box.hidden = true;
      box.innerHTML = "";
      shown = [];
      cursor = -1;
    }

    function jump(it) {
      var target = null;
      var scope = it.s ? document.getElementById(it.s) : null;
      if (scope) {
        var nodes = scope.querySelectorAll("[data-k]");
        for (var i = 0; i < nodes.length; i++) {
          if (nodes[i].getAttribute("data-k") === it.label) { target = nodes[i]; break; }
        }
      }
      if (!target) target = scope || document.getElementById(it.s);
      if (!target) return;
      scrollToEl(target);
      target.classList.remove("flash");
      // 强制重排，保证动画能重新播
      void target.offsetWidth;
      target.classList.add("flash");
      setTimeout(function () { target.classList.remove("flash"); }, 1600);
      input.blur();
      close();
      if (window.__closeDrawer) window.__closeDrawer();
    }

    function render(q) {
      var hits = match(q);
      box.innerHTML = "";
      shown = hits;
      cursor = hits.length ? 0 : -1;
      box.hidden = false;

      if (!hits.length) {
        var empty = document.createElement("div");
        empty.className = "hit-empty";
        empty.textContent = "没找到「" + q + "」";
        box.appendChild(empty);
        return;
      }

      hits.forEach(function (it, i) {
        var b = document.createElement("button");
        b.type = "button";
        b.className = "hit" + (i === 0 ? " is-cursor" : "");
        var k = document.createElement("span");
        k.className = "hit-k";
        k.appendChild(mark(it.label, q));
        var s = document.createElement("span");
        s.className = "hit-s";
        s.textContent = it.t + " · " + it.kind;
        b.appendChild(k);
        b.appendChild(s);
        b.addEventListener("click", function () { jump(it); });
        b.addEventListener("pointerenter", function () { setCursor(i); });
        box.appendChild(b);
      });
    }

    function setCursor(i) {
      if (!shown.length) return;
      cursor = (i + shown.length) % shown.length;
      var kids = box.querySelectorAll(".hit");
      for (var j = 0; j < kids.length; j++) kids[j].classList.toggle("is-cursor", j === cursor);
    }

    var timer = 0;
    input.addEventListener("input", function () {
      clearTimeout(timer);
      var q = input.value.trim();
      if (!q) { close(); return; }
      timer = setTimeout(function () { render(q); }, 90);
    });

    input.addEventListener("keydown", function (e) {
      if (e.key === "ArrowDown") { e.preventDefault(); setCursor(cursor + 1); }
      else if (e.key === "ArrowUp") { e.preventDefault(); setCursor(cursor - 1); }
      else if (e.key === "Enter") {
        if (cursor >= 0 && shown[cursor]) { e.preventDefault(); jump(shown[cursor]); }
      } else if (e.key === "Escape") {
        input.value = "";
        close();
        input.blur();
      }
    });

    input.addEventListener("blur", function () { setTimeout(close, 160); });

    // 按 "/" 直接跳到搜索框（窄屏先把抽屉拉出来）
    document.addEventListener("keydown", function (e) {
      if (e.key !== "/" || e.metaKey || e.ctrlKey || e.altKey) return;
      var t = e.target;
      if (t && (t.tagName === "INPUT" || t.tagName === "TEXTAREA" || t.isContentEditable)) return;
      e.preventDefault();
      var toggle = document.getElementById("nav-toggle");
      if (toggle && getComputedStyle(toggle).display !== "none" && !body.classList.contains("nav-open")) {
        toggle.click();
      }
      input.focus();
      input.select();
    });
  }

  /* ---------------- 平滑滚动（自己算，不依赖 scroll-behavior） ---------------- */

  var scrolling = null;

  // 元素在文档里的位置。用 offsetTop 逐层累加，而不是
  // getBoundingClientRect()+scrollY —— 后者会把"入场动画还没播完"的位移算进去，
  // 跳过去就会差个十几二十像素。
  function docTop(el) {
    var y = 0;
    for (var n = el; n; n = n.offsetParent) y += n.offsetTop;
    return y;
  }

  function scrollToEl(target, extra) {
    var offset = (extra || 0) + (window.innerWidth <= 1020 ? 70 : 24);
    var dist = docTop(target) - offset;
    var start = window.scrollY || window.pageYOffset || 0;
    var delta = dist - start;

    if (scrolling) scrolling();
    if (reduced || Math.abs(delta) < 4) {
      window.scrollTo(0, dist);
      return;
    }
    var dur = clamp(Math.abs(delta) * 0.5, 340, 950);
    scrolling = tween(dur, 0, ease.inOut, function (k) {
      window.scrollTo(0, start + delta * k);
    }, function () { scrolling = null; });
  }

  function initAnchors() {
    document.addEventListener("click", function (e) {
      var a = e.target.closest ? e.target.closest('a[href^="#"]') : null;
      if (!a) return;
      var id = a.getAttribute("href");
      if (!id || id === "#") return;
      var el = document.querySelector(id);
      if (!el) return;
      e.preventDefault();
      scrollToEl(el);
      try { history.replaceState(null, "", id); } catch (err) { }
    });
  }

  /* ---------------- 滚动进场 ---------------- */

  function initReveal() {
    var blocks = Array.prototype.slice.call(document.querySelectorAll("main > section.block"));
    if (!blocks.length) return;

    var queue = [];

    blocks.forEach(function (b) {
      var kids = b.querySelectorAll(
        ".skill-group, .about-bars, .card, .timeline > li, .prose > p, .prose > h3, .prose > ul, .prose > hr, .board, .block-lead"
      );
      queue.push({ el: b, d: 0, y: 34, kind: "block" });
      Array.prototype.forEach.call(kids, function (k, i) {
        queue.push({ el: k, d: 90 + i * 55, y: 20, kind: "kid", parent: b });
      });
    });

    // 首屏文字：立即错峰进场
    var heroBits = document.querySelectorAll(".hero-avatar, .hero-text > *");
    Array.prototype.forEach.call(heroBits, function (el, i) {
      el.style.opacity = "0";
      tween(700, i * 80, ease.out, function (k) {
        el.style.opacity = String(k);
        el.style.transform = "translateY(" + ((1 - k) * 18).toFixed(2) + "px)";
      }, function () { el.style.transform = ""; });
    });

    queue.forEach(function (item) {
      if (item.el.classList.contains("hero")) return;
      item.el.style.opacity = "0";
      item.el.style.transform = "translateY(" + item.y + "px)";
    });

    var live = queue.slice();

    function startItem(item) {
      if (item.done) return;
      item.done = true;
      item.el.classList.add("in");
      tween(item.kind === "block" ? 780 : 640, reduced ? 0 : item.d, ease.out,
        function (k) {
          item.el.style.opacity = String(k);
          item.el.style.transform = "translateY(" + ((1 - k) * item.y).toFixed(2) + "px)";
        },
        function () {
          item.el.style.opacity = "";
          item.el.style.transform = "";
          // 技能大框和关于我最下面的自评条：进场到位后再让进度条长出来
          if (item.kind === "kid" && window.__skinSkills &&
              (item.el.classList.contains("skill-group") || item.el.classList.contains("about-bars"))) {
            window.__skinSkills(item.el);
          }
        });
    }

    function sweep() {
      var h = window.innerHeight || 0;
      for (var i = live.length - 1; i >= 0; i--) {
        var it = live[i];
        var r = it.el.getBoundingClientRect();
        // 已经滚过去（在视口上方）或刚进入视口下缘：都算进场
        if (r.top < h * 0.9 || r.bottom < 0) {
          startItem(it);
          live.splice(i, 1);
        }
      }
    }

    var sweeping = false;
    function onScroll() {
      if (sweeping) return;
      sweeping = true;
      raf(function () { sweeping = false; sweep(); });
    }
    window.addEventListener("scroll", onScroll, { passive: true });
    window.addEventListener("resize", onScroll);
    raf(sweep);
  }

  /* ---------------- 首屏打字机 ---------------- */

  function initTypewriter() {
    var h1 = document.querySelector(".hero h1");
    if (!h1) return;
    var name = (h1.textContent || "").trim();
    if (!name) return;

    h1.setAttribute("aria-label", name);
    var face = document.createElement("span");
    face.className = "grad";
    var caret = document.createElement("i");
    caret.className = "type-caret";
    caret.setAttribute("aria-hidden", "true");
    h1.textContent = "";
    h1.appendChild(face);
    h1.appendChild(caret);

    if (reduced) { face.textContent = name; caret.classList.add("hide"); return; }

    var i = 0;
    (function step() {
      if (i >= name.length) {
        setTimeout(function () { caret.classList.add("hide"); }, 1500);
        return;
      }
      face.textContent += name.charAt(i++);
      setTimeout(step, 80 + Math.random() * 60);
    })();
  }

  /* ---------------- 技能条：逐帧补间 + 数字滚动 ---------------- */

  function skinSkills(scope) {
    var fills = scope.querySelectorAll(".skill-fill");
    Array.prototype.forEach.call(fills, function (f, i) {
      var pct = parseInt(f.getAttribute("data-pct"), 10) || 0;
      var head = f.parentNode.previousElementSibling;
      var num = head ? head.querySelector(".skill-pct") : null;
      f.style.width = "0%";
      if (num) num.textContent = "0%";

      tween(950, i * 70, ease.spring, function (k) {
        // 弹簧会过冲，进度条和数字都不能跟着冲过 100%
        var kk = Math.min(k, 1);
        f.style.width = (pct * kk).toFixed(1) + "%";
        if (num) num.textContent = Math.round(pct * kk) + "%";
      }, function () {
        f.style.width = pct + "%";
        if (num) num.textContent = pct + "%";
      });
    });
  }
  window.__skinSkills = skinSkills;

  /* ---------------- 卡片光晕跟随鼠标 ---------------- */

  function initCardGlow() {
    var fine = window.matchMedia ? window.matchMedia("(pointer: fine)").matches : false;
    if (!fine) return;
    var cards = document.querySelectorAll(".card");
    Array.prototype.forEach.call(cards, function (card) {
      card.addEventListener("pointermove", function (e) {
        var r = card.getBoundingClientRect();
        card.style.setProperty("--mx", ((e.clientX - r.left) / r.width * 100).toFixed(1) + "%");
        card.style.setProperty("--my", ((e.clientY - r.top) / r.height * 100).toFixed(1) + "%");
      }, { passive: true });
    });
  }

  /* ---------------- 首屏视差 ---------------- */

  function initParallax() {
    var hero = document.querySelector(".hero");
    if (!hero) return;
    var fine = window.matchMedia ? window.matchMedia("(pointer: fine)").matches : false;

    var mx = 0, my = 0, cx = 0, cy = 0, scrollK = 0, running = false;

    if (fine && !reduced) {
      hero.addEventListener("pointermove", function (e) {
        var r = hero.getBoundingClientRect();
        mx = ((e.clientX - r.left) / r.width - 0.5) * 20;
        my = ((e.clientY - r.top) / r.height - 0.5) * 13;
      }, { passive: true });
      hero.addEventListener("pointerleave", function () { mx = 0; my = 0; });
    }

    function loop() {
      cx = lerp(cx, mx, reduced ? 1 : 0.08);
      cy = lerp(cy, my, reduced ? 1 : 0.08);
      var settle = Math.abs(cx - mx) < 0.05 && Math.abs(cy - my) < 0.05;
      if (settle) { cx = mx; cy = my; }
      hero.style.setProperty("--px", cx.toFixed(2) + "px");
      hero.style.setProperty("--py", (cy + scrollK * 42).toFixed(2) + "px");
      if (settle && scrollK === 0) { running = false; return; }
      raf(loop);
    }

    function onScroll() {
      var h = hero.offsetHeight || 1;
      scrollK = clamp((window.scrollY || 0) / h, 0, 1);
      if (!running) { running = true; raf(loop); }
    }

    window.addEventListener("scroll", onScroll, { passive: true });
    onScroll();
    if (fine && !reduced) { running = true; raf(loop); }
  }

  /* ---------------- 进场门 ---------------- */

  // 返回 true 表示"这次确实有门要过"，门过完了才调用 done(target)
  function initGate(done) {
    var gate = document.getElementById("gate");
    if (!gate || !root.classList.contains("gate-on")) return false;

    var slides = Array.prototype.slice.call(gate.querySelectorAll(".gate-slide"));
    var dots = Array.prototype.slice.call(gate.querySelectorAll(".gate-dots button"));
    var cur = 0, auto = 0, alive = true;

    function show(next, dir, instant) {
      next = (next + slides.length) % slides.length;
      if (next === cur && !instant) return;
      var prev = slides[cur];
      var nowS = slides[next];
      dir = dir || 1;
      cur = next;

      dots.forEach(function (d, i) { d.classList.toggle("on", i === cur); });

      slides.forEach(function (s) {
        if (s !== nowS && s !== prev) { s.style.opacity = "0"; s.style.pointerEvents = "none"; }
      });

      nowS.style.pointerEvents = "";
      if (prev !== nowS) prev.style.pointerEvents = "none";

      // 首次调用时 prev 和 nowS 是同一张，别把自己又按回去
      if (instant || reduced) {
        nowS.style.opacity = "1";
        nowS.style.transform = "";
        if (prev !== nowS) {
          prev.style.opacity = "0";
          prev.style.transform = "";
        }
        return;
      }
      tween(430, 0, ease.out, function (k) {
        nowS.style.opacity = String(k);
        nowS.style.transform = "translateX(" + ((1 - k) * dir * 6).toFixed(2) + "%)";
      }, function () { nowS.style.transform = ""; });

      if (prev !== nowS) {
        tween(320, 0, ease.out, function (k) {
          prev.style.opacity = String(1 - k);
          prev.style.transform = "translateX(" + (-k * dir * 6).toFixed(2) + "%)";
        }, function () { prev.style.transform = ""; });
      }
    }

    function tick() {
      if (!alive) return;
      auto = setTimeout(function () { show(cur + 1, 1); tick(); }, 7000);
    }

    dots.forEach(function (d, i) {
      d.addEventListener("click", function () {
        clearTimeout(auto);
        show(i, i > cur ? 1 : -1);
        tick();
      });
    });

    // 键盘左右切换
    document.addEventListener("keydown", function (e) {
      if (!root.classList.contains("gate-on")) return;
      if (e.key === "ArrowRight") { clearTimeout(auto); show(cur + 1, 1); tick(); }
      if (e.key === "ArrowLeft") { clearTimeout(auto); show(cur - 1, -1); tick(); }
    });

    // 鼠标停在门上的时候不要自动翻页
    gate.addEventListener("pointerenter", function () { clearTimeout(auto); });
    gate.addEventListener("pointerleave", function () { clearTimeout(auto); tick(); });

    // 触摸左右滑动
    var tx = 0;
    gate.addEventListener("touchstart", function (e) {
      tx = e.touches[0].clientX;
      clearTimeout(auto);
    }, { passive: true });
    gate.addEventListener("touchend", function (e) {
      var dx = e.changedTouches[0].clientX - tx;
      if (Math.abs(dx) > 42) show(cur + (dx < 0 ? 1 : -1), dx < 0 ? 1 : -1);
      tick();
    }, { passive: true });

    show(0, 1, true);
    tick();

    function proceed(target) {
      alive = false;
      clearTimeout(auto);
      gate.classList.add("out");
      setTimeout(function () {
        gate.style.display = "none";
        root.classList.remove("gate-on");
        body.classList.remove("nav-open");
        done(target);
      }, reduced ? 0 : 400);
    }

    var btn = document.getElementById("gate-btn");
    if (btn) {
      btn.addEventListener("click", function () {
        btn.classList.add("pressed");
        // 按钮上的高光扫过之后再走，动作有先后顺序
        setTimeout(function () { proceed(null); }, reduced ? 0 : 320);
      });
    }

    gate.querySelectorAll(".gate-links a").forEach(function (a) {
      a.addEventListener("click", function (e) {
        var href = a.getAttribute("href") || "";
        if (/^(https?:|mailto:)/i.test(href)) { proceed(null); return; }
        e.preventDefault();
        proceed(href);
      });
    });

    // 直接在门上滚轮也能翻页
    gate.addEventListener("wheel", function (e) {
      if (Math.abs(e.deltaY) < 24) return;
      clearTimeout(auto);
      show(cur + (e.deltaY > 0 ? 1 : -1), e.deltaY > 0 ? 1 : -1);
      tick();
    }, { passive: true });

    return true;
  }

  /* ---------------- 启动 ---------------- */

  var started = false;

  function startFx() {
    if (!window.FX) return;
    FX.retheme();
    var fire = document.getElementById("fire-canvas");
    if (fire) FX.fire.start(fire, { reduced: reduced });
    var cursor = document.getElementById("cursor-canvas");
    if (cursor && !reduced) FX.cursor.start(cursor);

    var chip = document.getElementById("chip-canvas");
    var board = document.getElementById("core-board");
    if (chip && board) {
      FX.chip.start(chip, {
        reduced: reduced,
        label: board.getAttribute("data-label") || "",
        sub: board.getAttribute("data-sub") || ""
      });
    }
  }

  // 门上的像素吉祥物：点阵由 C++ 给出（见 src/main.cpp 里的字符网格），
  // 这里只负责按调色板上色 + 待机动画。门一显示就要画好，不能等进门之后。
  function paintMascot() {
    var mascot = document.querySelector(".gate-strip canvas");
    var spriteEl = document.getElementById("sprite-data");
    if (!mascot || !spriteEl || !window.FX) return;
    try {
      FX.sprite.paint(mascot, JSON.parse(spriteEl.textContent || "{}"), true);
    } catch (e) { }
  }

  // 进门之后的赛博像素 WELCOME：动画放完再把页面点亮。
  // welcome = always 时就算系统关了动效也照播；welcome = auto 才跟随系统。
  function withWelcome(then) {
    var w = document.getElementById("welcome");
    if (!w || !window.FX || typeof FX.welcome !== "function") { then(); return; }
    if (reduced && !root.classList.contains("welcome-always")) { then(); return; }
    FX.welcome(w, "WELCOME!", then);
  }

  function boot() {
    // 这几件事得在门还盖着的时候就绪
    initTheme();
    initDrawer();
    initAnchors();
    paintMascot();

    var spy = Array.prototype.slice.call(document.querySelectorAll("main > section.block"));

    // 门之后的正式开场：导航、查找、进场编排、打字机、视差、特效
    function afterGate(target) {
      if (started) return;
      started = true;

      initNav(spy);
      initSearch();
      initTypewriter();
      initReveal();
      initCardGlow();
      initParallax();
      startFx();

      if (target && target.charAt(0) === "#") {
        var el = document.querySelector(target);
        if (el) setTimeout(function () { scrollToEl(el); }, 60);
      }
    }

    var gated = initGate(function (target) {
      withWelcome(function () { afterGate(target); });
    });

    if (!gated) withWelcome(function () { afterGate(null); });
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", boot);
  } else {
    boot();
  }
})();
