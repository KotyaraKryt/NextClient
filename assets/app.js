(function () {
  'use strict';

  // the repository whose releases the site offers and lists
  var kRepo = 'CS-NextClient/NextClient';
  var kReleasesUrl = 'https://github.com/' + kRepo + '/releases';
  var kApiUrl = 'https://api.github.com/repos/' + kRepo;
  // without a token GitHub answers 60 requests an hour per address, so one answer serves the whole visit
  var kCacheKey = 'site:2:' + kRepo;
  var kCacheSeconds = 600;
  var kShownAtFirst = 4;

  var kReducedMotion = !!(window.matchMedia && window.matchMedia('(prefers-reduced-motion: reduce)').matches);
  var kFinePointer = !!(window.matchMedia && window.matchMedia('(hover: hover) and (pointer: fine)').matches);

  var releases = null;
  var downloads = null;
  var stars = null;
  var showAll = false;

  function forEach(selector, fn) {
    Array.prototype.forEach.call(document.querySelectorAll(selector), fn);
  }

  function lang() {
    return document.documentElement.getAttribute('data-lang') === 'ru' ? 'ru' : 'en';
  }

  function text(ru, en) {
    return lang() === 'ru' ? ru : en;
  }

  function setLang(value) {
    document.documentElement.setAttribute('data-lang', value);
    document.documentElement.lang = value;
    try { localStorage.setItem('lang', value); } catch (e) {}
    render();
  }

  document.getElementById('lang-switch').addEventListener('click', function () {
    setLang(lang() === 'ru' ? 'en' : 'ru');
  });

  document.getElementById('more').addEventListener('click', function () {
    showAll = true;
    renderReleases();
  });

  // markdown, the little of it release notes use

  function escapeHtml(value) {
    return value.replace(/[&<>"']/g, function (c) {
      return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c];
    });
  }

  function inline(value) {
    var codes = [];
    var html = escapeHtml(value).replace(/`([^`]+)`/g, function (_, code) {
      codes.push('<code>' + code + '</code>');
      return '\u0000' + (codes.length - 1) + '\u0000';
    });

    html = html
      .replace(/\*\*([^*]+)\*\*/g, '<b>$1</b>')
      .replace(/\[([^\]]+)\]\((https?:\/\/[^\s)]+)\)/g, '<a href="$2" target="_blank" rel="noopener">$1</a>')
      .replace(/(^|[\s(])(https?:\/\/[^\s)<]+)/g, '$1<a href="$2" target="_blank" rel="noopener">$2</a>')
      .replace(/(^|[\s(])#(\d+)\b/g, '$1<a href="https://github.com/' + kRepo + '/issues/$2" target="_blank" rel="noopener">#$2</a>');

    return html.replace(/\u0000(\d+)\u0000/g, function (_, i) {
      return codes[+i];
    });
  }

  function markdown(source) {
    var lines = source.replace(/\r\n?/g, '\n').split('\n');
    var out = [];
    var paragraph = [];
    var list = null;

    function flushParagraph() {
      if (paragraph.length) {
        out.push('<p>' + inline(paragraph.join(' ')) + '</p>');
        paragraph = [];
      }
    }

    function flushList() {
      if (list) {
        out.push('<' + list.tag + '>' + list.items.map(function (item) {
          return '<li>' + inline(item) + '</li>';
        }).join('') + '</' + list.tag + '>');
        list = null;
      }
    }

    lines.forEach(function (line) {
      var heading = /^\s*#{1,6}\s+(.*)$/.exec(line);
      var item = /^\s*(\d+[.)]|[-*+])\s+(.*)$/.exec(line);

      if (!line.trim()) {
        flushParagraph();
        return;
      }

      if (heading) {
        flushParagraph();
        flushList();
        out.push('<h4>' + inline(heading[1]) + '</h4>');
      } else if (item) {
        flushParagraph();
        var tag = /\d/.test(item[1]) ? 'ol' : 'ul';
        if (list && list.tag !== tag && !/^\s{2,}/.test(line)) {
          flushList();
        }
        if (!list) {
          list = { tag: tag, items: [] };
        }
        list.items.push(item[2]);
      } else if (list && /^\s+/.test(line)) {
        // a wrapped line of the item above
        list.items[list.items.length - 1] += ' ' + line.trim();
      } else {
        flushList();
        paragraph.push(line.trim());
      }
    });

    flushParagraph();
    flushList();
    return out.join('');
  }

  // the notes are written twice, under "## EN" and "## RU"; the other language stands in when one is missing
  function notesFor(body, language) {
    var sections = {};
    var current = null;
    (body || '').replace(/\r\n?/g, '\n').split('\n').forEach(function (line) {
      var head = /^##\s*(EN|RU)\s*$/i.exec(line.trim());
      if (head) {
        current = head[1].toLowerCase();
        sections[current] = [];
      } else if (current) {
        sections[current].push(line);
      }
    });

    var chosen = sections[language] || sections[language === 'ru' ? 'en' : 'ru'];
    return chosen ? chosen.join('\n').trim() : (body || '').trim();
  }

  function formatDate(iso) {
    try {
      return new Date(iso).toLocaleDateString(lang() === 'ru' ? 'ru-RU' : 'en-US', { day: 'numeric', month: 'long', year: 'numeric' });
    } catch (e) {
      return iso.slice(0, 10);
    }
  }

  function formatSize(bytes) {
    return (bytes / 1048576).toFixed(1) + (lang() === 'ru' ? ' МБ' : ' MB');
  }

  // the game's archive, its Linux one when there is one; never the debug symbols
  function pickAssets(release) {
    var assets = (release.assets || []).filter(function (asset) {
      return !/pdb/i.test(asset.name);
    });
    return {
      windows: assets.find(function (asset) { return /\.zip$/i.test(asset.name); }),
      linux: assets.find(function (asset) { return /linux.*\.tar\.gz$|\.tar\.gz$/i.test(asset.name); })
    };
  }

  function versionOf(release) {
    return (release.tag_name || '').replace(/^v/i, '');
  }

  // 'windows', 'linux' or 'mac'; Android says Linux but gets no build
  function detectOs() {
    var platform = (navigator.userAgentData && navigator.userAgentData.platform) || navigator.platform || '';
    var agent = navigator.userAgent || '';
    if (/android/i.test(agent)) {
      return 'other';
    }
    if (/mac|iphone|ipad/i.test(platform) || /mac os x/i.test(agent)) {
      return 'mac';
    }
    if (/linux|x11|cros/i.test(platform) || /linux|cros/i.test(agent)) {
      return 'linux';
    }
    return 'windows';
  }

  var kOs = detectOs();
  var kOsNames = { windows: 'Windows', linux: 'Linux' };

  function assetLink(asset, os) {
    return '<a href="' + asset.browser_download_url + '">' + text('сборка для ', 'the ') + kOsNames[os]
      + text('', ' build') + '</a> (' + formatSize(asset.size) + ')';
  }

  // the visitor's own system first, Windows when there's no build for it
  function offer(release) {
    var assets = pickAssets(release);
    var os = kOs === 'linux' && assets.linux ? 'linux' : (assets.windows ? 'windows' : (assets.linux ? 'linux' : null));
    return { assets: assets, os: os, asset: os ? assets[os] : null };
  }

  function renderDownload() {
    var others = document.getElementById('other-downloads');
    var latest = releases && releases[0];
    if (!latest) {
      return;
    }

    var choice = offer(latest);
    var assets = choice.assets;
    var mainOs = choice.os;
    var main = choice.asset;
    var otherOs = mainOs === 'windows' ? 'linux' : 'windows';

    forEach('[data-download]', function (button) {
      button.href = main ? main.browser_download_url : latest.html_url;
    });
    forEach('[data-download-meta]', function (meta) {
      meta.textContent = 'v' + versionOf(latest) + (main ? ' · ' + formatSize(main.size) + ' · ' + kOsNames[mainOs] : '');
    });

    var note = '';
    if (kOs === 'linux' && !assets.linux) {
      note = text('Сборка для Linux появится в следующем релизе.', 'A Linux build is coming in the next release.');
    } else if (kOs === 'mac') {
      note = text('Для macOS сборки нет.', 'There is no macOS build.');
    }
    if (main && assets[otherOs]) {
      note += (note ? ' ' : '') + text('Есть и ', 'There is also ') + assetLink(assets[otherOs], otherOs) + '.';
    }

    others.innerHTML = note;
    others.hidden = !note;
  }

  // 1 скачивание, 2 скачивания, 5 скачиваний
  function plural(n, one, few, many) {
    var tens = n % 100;
    var units = n % 10;
    if (tens >= 11 && tens <= 14) {
      return many;
    }
    return units === 1 ? one : (units >= 2 && units <= 4 ? few : many);
  }

  function daysSince(iso) {
    return Math.max(0, Math.floor((Date.now() - new Date(iso).getTime()) / 86400000));
  }

  // "16 дней назад", "yesterday"
  function ago(iso) {
    var days = daysSince(iso);
    try {
      var format = new Intl.RelativeTimeFormat(lang() === 'ru' ? 'ru' : 'en', { numeric: 'auto' });
      return days < 60 ? format.format(-days, 'day') : format.format(-Math.round(days / 30), 'month');
    } catch (e) {
      return iso.slice(0, 10);
    }
  }

  function number(value) {
    return value.toLocaleString(lang() === 'ru' ? 'ru-RU' : 'en-US');
  }

  // what others already did, and that the project is alive: right under the buttons
  function renderStats() {
    var latest = releases && releases[0];
    var parts = [];
    if (downloads) {
      parts.push('<span><b>' + number(downloads) + '</b> ' + text(plural(downloads, 'скачивание', 'скачивания', 'скачиваний'), downloads === 1 ? 'download' : 'downloads') + '</span>');
    }
    if (stars) {
      parts.push('<span><b>★ ' + number(stars) + '</b> ' + text('на GitHub', 'on GitHub') + '</span>');
    }
    if (latest) {
      parts.push('<span class="live">' + text('обновлено ', 'updated ') + '<b>' + ago(latest.published_at) + '</b></span>');
    }
    forEach('#proof, #final-proof', function (box) {
      box.innerHTML = parts.join('');
      box.hidden = !parts.length;
    });

    var label = document.querySelector('[data-fresh-label]');
    if (latest && label) {
      var days = daysSince(latest.published_at);
      label.innerHTML = text(plural(days, 'день', 'дня', 'дней') + ' с обновления', (days === 1 ? 'day' : 'days') + ' since the update');
    }
    countUp();
  }

  // the numbers band counts up once it's on screen and the numbers are in
  var counted = false;
  var numbersSeen = false;

  function countUp() {
    var latest = releases && releases[0];
    if (counted || !numbersSeen || !latest) {
      return;
    }
    counted = true;

    var targets = { downloads: downloads || 0, stars: stars || 0, releases: releases.length, fresh: daysSince(latest.published_at) };
    forEach('[data-count]', function (el) {
      var target = targets[el.getAttribute('data-count')] || 0;
      if (kReducedMotion) {
        el.textContent = number(target);
        return;
      }
      var start = performance.now();
      var duration = 1600;
      (function step(now) {
        var k = Math.min(1, (now - start) / duration);
        // fast at first, settling at the end
        el.textContent = number(Math.round(target * (1 - Math.pow(1 - k, 4))));
        if (k < 1) {
          requestAnimationFrame(step);
        }
      })(start);
    });
  }

  // the main button's archive, to check against what was downloaded
  function renderChecksum() {
    var box = document.getElementById('checksum');
    var button = document.getElementById('checksum-copy');
    var latest = releases && releases[0];
    var asset = latest ? offer(latest).asset : null;
    var hash = asset && /^sha256:[0-9a-f]{64}$/.test(asset.digest || '') ? asset.digest.slice(7) : '';
    box.hidden = !hash;
    if (!hash) {
      return;
    }
    button.textContent = hash.slice(0, 10) + '…' + hash.slice(-10);
    button.title = hash;
    button.dataset.hash = hash;
    button.setAttribute('aria-label', text('Скопировать SHA-256', 'Copy SHA-256'));
  }

  document.getElementById('checksum-copy').addEventListener('click', function () {
    var button = this;
    if (!navigator.clipboard || !button.dataset.hash) {
      return;
    }
    navigator.clipboard.writeText(button.dataset.hash).then(function () {
      var shown = button.textContent;
      button.textContent = text('Скопировано', 'Copied');
      button.classList.add('copied');
      setTimeout(function () {
        button.textContent = shown;
        button.classList.remove('copied');
      }, 1400);
    });
  });

  function renderReleases() {
    var container = document.getElementById('releases');
    var more = document.getElementById('more');

    if (releases === null) {
      return;
    }

    if (!releases.length) {
      container.innerHTML = '<p class="releases-error">'
        + text('Не удалось загрузить список изменений. Он есть на ', 'Could not load the changelog. It is on ')
        + '<a href="' + kReleasesUrl + '" target="_blank" rel="noopener">GitHub</a>.</p>';
      more.hidden = true;
      return;
    }

    var shown = showAll ? releases : releases.slice(0, kShownAtFirst);
    container.innerHTML = shown.map(function (release, index) {
      var version = versionOf(release);
      var title = release.name && release.name.indexOf(version) === -1 ? release.name : '';
      return '<article class="release' + (index === 0 ? ' latest' : '') + '" id="v' + escapeHtml(version) + '">'
        + '<div class="release-head">'
        + '<h3 class="release-version"><a href="#v' + escapeHtml(version) + '">' + escapeHtml(version) + '</a></h3>'
        + (index === 0 ? '<span class="badge">' + text('актуальная', 'latest') + '</span>' : '')
        + '<span class="release-date">' + formatDate(release.published_at) + '</span>'
        + (title ? '<span class="release-title">' + escapeHtml(title) + '</span>' : '')
        + '</div>'
        + '<div class="release-body">' + markdown(notesFor(release.body, lang())) + '</div>'
        + '<div class="release-foot"><a href="' + release.html_url + '" target="_blank" rel="noopener">'
        + text('Релиз на GitHub', 'Release on GitHub') + ' →</a></div>'
        + '</article>';
    }).join('');

    more.hidden = showAll || releases.length <= kShownAtFirst;
  }

  function render() {
    renderDownload();
    renderStats();
    renderChecksum();
    renderReleases();
  }

  // a link to a version further down the list opens the whole list
  function revealHash() {
    var id = decodeURIComponent(location.hash.slice(1));
    var question = /^faq-/.test(id) ? document.getElementById(id) : null;
    if (question && question.tagName === 'DETAILS') {
      question.open = true;
      question.scrollIntoView({ block: 'start' });
      return;
    }
    if (!/^v[\w.-]+$/.test(id) || !releases) {
      return;
    }
    if (!document.getElementById(id) && releases.some(function (release) { return 'v' + versionOf(release) === id; })) {
      showAll = true;
      renderReleases();
    }
    var target = document.getElementById(id);
    if (target) {
      target.scrollIntoView({ block: 'start' });
      target.classList.remove('highlight');
      void target.offsetWidth;
      target.classList.add('highlight');
    }
  }

  window.addEventListener('hashchange', revealHash);

  function readCache() {
    try {
      var cached = JSON.parse(sessionStorage.getItem(kCacheKey));
      if (cached && Date.now() - cached.time < kCacheSeconds * 1000) {
        return cached;
      }
    } catch (e) {}
    return null;
  }

  function writeCache() {
    try {
      sessionStorage.setItem(kCacheKey, JSON.stringify({ time: Date.now(), releases: releases, downloads: downloads, stars: stars }));
    } catch (e) {}
  }

  // every archive of every release, test builds included; the debug symbols aren't the game
  function countDownloads(list) {
    return list.reduce(function (sum, release) {
      return sum + (release.assets || []).reduce(function (n, asset) {
        return n + (/pdb/i.test(asset.name) ? 0 : asset.download_count || 0);
      }, 0);
    }, 0);
  }

  function getJson(url) {
    return fetch(url, { headers: { Accept: 'application/vnd.github+json' } }).then(function (response) {
      if (!response.ok) {
        throw new Error('HTTP ' + response.status);
      }
      return response.json();
    });
  }

  // only what the page shows is kept, so the cache stays small
  function trim(list) {
    return list.filter(function (release) {
      return !release.draft && !release.prerelease;
    }).map(function (release) {
      return {
        tag_name: release.tag_name,
        name: release.name,
        body: release.body,
        html_url: release.html_url,
        published_at: release.published_at,
        assets: (release.assets || []).map(function (asset) {
          return { name: asset.name, size: asset.size, digest: asset.digest, browser_download_url: asset.browser_download_url };
        })
      };
    });
  }

  function load() {
    var cached = readCache();
    if (cached) {
      releases = cached.releases;
      downloads = cached.downloads;
      stars = cached.stars;
      render();
      revealHash();
      return;
    }

    // the stars are a nicety: the page goes on without them
    var repo = getJson(kApiUrl).then(function (info) {
      stars = info.stargazers_count;
    }, function () {});

    var list = getJson(kApiUrl + '/releases?per_page=100').then(function (all) {
      releases = trim(all);
      downloads = countDownloads(all);
    });

    Promise.all([list, repo])
      .then(writeCache, function () {
        releases = [];
      })
      .then(function () {
        render();
        revealHash();
      });
  }

  // the tour: a game window that stays in place while the steps scroll past it

  function initTour() {
    var box = document.getElementById('lightbox');
    var windowButton = document.getElementById('tour-window');
    var section = document.getElementById('tour');
    if (!box || !section) {
      return;
    }

    var steps = Array.prototype.slice.call(section.querySelectorAll('.tour-step'));
    var shots = Array.prototype.slice.call(section.querySelectorAll('.tour-shot'));
    var progress = document.getElementById('tour-progress');
    var current = 0;

    function activate(index) {
      current = index;
      steps.forEach(function (step, i) {
        step.classList.toggle('active', i === index);
      });
      shots.forEach(function (shot, i) {
        shot.classList.toggle('active', i === index);
      });
    }

    // the step that crosses the middle of the screen is the one shown
    if ('IntersectionObserver' in window) {
      var observer = new IntersectionObserver(function (entries) {
        entries.forEach(function (entry) {
          if (entry.isIntersecting) {
            activate(steps.indexOf(entry.target));
          }
        });
      }, { rootMargin: '-50% 0px -50% 0px' });
      steps.forEach(function (step) {
        observer.observe(step);
      });
    }

    onScroll(function () {
      var list = section.querySelector('.tour-steps').getBoundingClientRect();
      var middle = window.innerHeight / 2;
      var p = Math.min(1, Math.max(0, (middle - list.top) / list.height));
      progress.style.setProperty('--p', p.toFixed(4));
    });

    // the full-size view

    var boxImage = box.querySelector('img');
    var boxCaption = box.querySelector('.lightbox-caption');
    var boxIndex = 0;

    function openBox(index) {
      boxIndex = (index + steps.length) % steps.length;
      var step = steps[boxIndex];
      boxImage.src = step.querySelector('img').src;
      boxCaption.innerHTML = '<b>' + step.querySelector('h3').innerHTML + '</b> · ' + step.querySelector('p').innerHTML;
      if (!box.open) {
        box.showModal();
      }
    }

    windowButton.addEventListener('click', function () {
      openBox(current);
    });
    forEach('[data-open]', function (button) {
      button.addEventListener('click', function () {
        openBox(+button.getAttribute('data-open'));
      });
    });

    box.querySelector('.lightbox-close').addEventListener('click', function () {
      box.close();
    });
    box.querySelector('.lightbox-prev').addEventListener('click', function () {
      openBox(boxIndex - 1);
    });
    box.querySelector('.lightbox-next').addEventListener('click', function () {
      openBox(boxIndex + 1);
    });
    box.addEventListener('click', function (event) {
      if (event.target === box) {
        box.close();
      }
    });
    box.addEventListener('keydown', function (event) {
      if (event.key === 'ArrowLeft') {
        openBox(boxIndex - 1);
      } else if (event.key === 'ArrowRight') {
        openBox(boxIndex + 1);
      }
    });

    var touchX = null;
    box.addEventListener('touchstart', function (event) {
      touchX = event.touches[0].clientX;
    }, { passive: true });
    box.addEventListener('touchend', function (event) {
      if (touchX === null) {
        return;
      }
      var dx = event.changedTouches[0].clientX - touchX;
      touchX = null;
      if (Math.abs(dx) > 50) {
        openBox(boxIndex + (dx < 0 ? 1 : -1));
      }
    });

    tilt(windowButton, 5, 1400);
  }

  // an element leans towards the pointer, like a card held in the hand
  function tilt(el, degrees, perspective) {
    if (!el || !kFinePointer || kReducedMotion) {
      return;
    }
    el.addEventListener('pointermove', function (event) {
      var rect = el.getBoundingClientRect();
      var x = (event.clientX - rect.left) / rect.width - 0.5;
      var y = (event.clientY - rect.top) / rect.height - 0.5;
      el.style.transform = 'perspective(' + perspective + 'px) rotateY(' + (x * degrees).toFixed(2) + 'deg) rotateX(' + (-y * degrees).toFixed(2) + 'deg)';
    });
    el.addEventListener('pointerleave', function () {
      el.style.transform = '';
    });
  }

  // one scroll listener and one frame for everything that follows the scroll
  var scrollHandlers = [];
  var scrollFrame = 0;

  function onScroll(fn) {
    scrollHandlers.push(fn);
  }

  function runScroll() {
    scrollFrame = 0;
    scrollHandlers.forEach(function (fn) {
      fn(window.scrollY);
    });
  }

  function requestScroll() {
    if (!scrollFrame) {
      scrollFrame = requestAnimationFrame(runScroll);
    }
  }

  window.addEventListener('scroll', requestScroll, { passive: true });
  window.addEventListener('resize', requestScroll);

  // parallax: each [data-depth] layer of the hero moves with the scroll and the pointer by its depth,
  // a negative one against them; further down, [data-speed] elements drift as they pass the screen

  function initParallax() {
    var hero = document.querySelector('.hero');
    if (!hero || kReducedMotion) {
      return;
    }

    var layers = Array.prototype.slice.call(hero.querySelectorAll('[data-depth]')).map(function (el) {
      return {
        el: el,
        depth: parseFloat(el.getAttribute('data-depth')) || 0,
        rotate: parseFloat(el.getAttribute('data-rotate')) || 0,
        tilt: el.hasAttribute('data-tilt')
      };
    });

    var pointerX = 0;
    var pointerY = 0;
    var easedX = 0;
    var easedY = 0;
    var frame = 0;

    function update() {
      frame = 0;
      // the layers drift after the pointer rather than jump with it
      easedX += (pointerX - easedX) * 0.08;
      easedY += (pointerY - easedY) * 0.08;
      var scroll = window.scrollY;

      if (scroll < hero.offsetTop + hero.offsetHeight) {
        layers.forEach(function (layer) {
          var x = easedX * layer.depth * 30;
          var y = easedY * layer.depth * 30 - scroll * layer.depth * 0.4;
          var transform = 'translate3d(' + x.toFixed(2) + 'px, ' + y.toFixed(2) + 'px, 0)';
          if (layer.rotate) {
            transform += ' rotate(' + (layer.rotate + scroll * layer.depth * 0.05).toFixed(2) + 'deg)';
          }
          if (layer.tilt) {
            transform += ' rotateY(' + (easedX * 12).toFixed(2) + 'deg) rotateX(' + (-easedY * 12).toFixed(2) + 'deg)';
          }
          layer.el.style.transform = transform;
        });
      }

      if (Math.abs(pointerX - easedX) > 0.001 || Math.abs(pointerY - easedY) > 0.001) {
        request();
      }
    }

    function request() {
      if (!frame) {
        frame = requestAnimationFrame(update);
      }
    }

    onScroll(request);

    if (kFinePointer) {
      window.addEventListener('pointermove', function (event) {
        pointerX = event.clientX / window.innerWidth * 2 - 1;
        pointerY = event.clientY / window.innerHeight * 2 - 1;
        request();
      }, { passive: true });
    }

    var drifting = Array.prototype.slice.call(document.querySelectorAll('[data-speed]'));
    onScroll(function () {
      var height = window.innerHeight;
      drifting.forEach(function (el) {
        var rect = el.parentNode.getBoundingClientRect();
        if (rect.bottom < -200 || rect.top > height + 200) {
          return;
        }
        // 0 when the element's box is in the middle of the screen
        var offset = rect.top + rect.height / 2 - height / 2;
        el.style.transform = 'translate3d(0, ' + (offset * parseFloat(el.getAttribute('data-speed'))).toFixed(1) + 'px, 0)';
      });
    });

    request();
  }

  // the reading progress, the header's line, the nav's current section and the header's own button
  function initPage() {
    var root = document.documentElement;
    var bar = document.getElementById('scroll-progress');
    onScroll(function (scroll) {
      var max = root.scrollHeight - window.innerHeight;
      bar.style.setProperty('--p', max > 0 ? (scroll / max).toFixed(4) : 0);
      root.classList.toggle('scrolled', scroll > 40);
    });

    if (!('IntersectionObserver' in window)) {
      root.classList.add('cta-away');
      return;
    }

    // the header offers the download once the hero's button is gone, and not over the final one
    var ctaSeen = { hero: true, final: false };
    var heroCta = document.querySelector('.hero [data-download]');
    var finalCta = document.querySelector('.final [data-download]');
    new IntersectionObserver(function (entries) {
      entries.forEach(function (entry) {
        ctaSeen[entry.target === heroCta ? 'hero' : 'final'] = entry.isIntersecting;
      });
      root.classList.toggle('cta-away', !ctaSeen.hero && !ctaSeen.final);
    }).observe(heroCta);
    new IntersectionObserver(function (entries) {
      ctaSeen.final = entries[0].isIntersecting;
      root.classList.toggle('cta-away', !ctaSeen.hero && !ctaSeen.final);
    }).observe(finalCta);

    var links = Array.prototype.slice.call(document.querySelectorAll('.links a'));
    var sectionObserver = new IntersectionObserver(function (entries) {
      entries.forEach(function (entry) {
        if (entry.isIntersecting) {
          links.forEach(function (link) {
            link.classList.toggle('current', link.getAttribute('href') === '#' + entry.target.id);
          });
        }
      });
    }, { rootMargin: '-45% 0px -50% 0px' });
    links.forEach(function (link) {
      var target = document.querySelector(link.getAttribute('href'));
      if (target) {
        sectionObserver.observe(target);
      }
    });

    new IntersectionObserver(function (entries) {
      if (entries[0].isIntersecting) {
        numbersSeen = true;
        countUp();
      }
    }, { threshold: 0.4 }).observe(document.querySelector('.numbers'));
  }

  // the sections come up as they scroll into view, the cards of a grid one after another
  function initReveal() {
    if (kReducedMotion || !('IntersectionObserver' in window)) {
      numbersSeen = true;
      return;
    }

    var groups = ['.section-head', '.cmp-row', '.feature', '.number', '.trust-item', '.steps', '.steps li', '.warning', '.faq-item', '.final-card'];
    var observer = new IntersectionObserver(function (entries) {
      entries.forEach(function (entry) {
        if (entry.isIntersecting) {
          entry.target.classList.add('in');
          observer.unobserve(entry.target);
        }
      });
    }, { threshold: 0.15, rootMargin: '0px 0px -60px 0px' });

    groups.forEach(function (selector) {
      forEach(selector, function (el, index) {
        el.classList.add('reveal');
        el.style.setProperty('--i', /section-head|final-card|^\.steps$/.test(selector) ? 0 : index % 6);
        observer.observe(el);
      });
    });
    document.documentElement.classList.add('reveal-ready');
  }

  forEach('[data-tilt-card]', function (card) {
    tilt(card, 6, 900);
  });

  initTour();
  initParallax();
  initPage();
  initReveal();
  load();
  runScroll();
})();
