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

  var releases = null;
  var downloads = null;
  var stars = null;
  var showAll = false;

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
    var button = document.getElementById('download');
    var meta = document.getElementById('download-meta');
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

    button.href = main ? main.browser_download_url : latest.html_url;
    meta.textContent = 'v' + versionOf(latest) + (main ? ' · ' + formatSize(main.size) + ' · ' + kOsNames[mainOs] : '');

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

  function renderStats() {
    var box = document.getElementById('stats');
    var parts = [];
    var locale = lang() === 'ru' ? 'ru-RU' : 'en-US';
    if (downloads) {
      parts.push('<span>' + downloads.toLocaleString(locale) + ' <small>'
        + text(plural(downloads, 'скачивание', 'скачивания', 'скачиваний'), downloads === 1 ? 'download' : 'downloads') + '</small></span>');
    }
    if (stars) {
      parts.push('<span><svg viewBox="0 0 24 24" aria-hidden="true"><path d="m12 2.5 2.9 6.1 6.6.8-4.9 4.6 1.3 6.5L12 17.3l-5.9 3.2 1.3-6.5-4.9-4.6 6.6-.8z"/></svg>'
        + stars.toLocaleString(locale) + ' <small>' + text(plural(stars, 'звезда', 'звезды', 'звёзд'), stars === 1 ? 'star' : 'stars')
        + text(' на GitHub', ' on GitHub') + '</small></span>');
    }
    box.innerHTML = parts.join('');
    box.hidden = !parts.length;
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

  // screenshots: a list beside a game window, turning by itself until the visitor picks one

  var kShotSeconds = 6;

  function initShowcase() {
    var showcase = document.getElementById('showcase');
    var box = document.getElementById('lightbox');
    if (!showcase || !box) {
      return;
    }

    var shots = Array.prototype.slice.call(showcase.querySelectorAll('.shot'));
    var tabsBox = showcase.querySelector('.shot-tabs');
    var reducedMotion = window.matchMedia && window.matchMedia('(prefers-reduced-motion: reduce)').matches;
    var current = 0;
    var playing = !reducedMotion;
    var visible = false;
    var timer = null;

    showcase.style.setProperty('--shot-time', kShotSeconds + 's');

    var tabs = shots.map(function (shot, index) {
      var tab = document.createElement('button');
      tab.type = 'button';
      tab.className = 'shot-tab';
      tab.setAttribute('role', 'tab');
      tab.innerHTML = shot.querySelector('figcaption').innerHTML;
      tab.addEventListener('click', function () {
        stop();
        show(index);
      });
      tabsBox.appendChild(tab);

      shot.querySelector('.shot-open').addEventListener('click', function () {
        stop();
        openBox(index);
      });
      return tab;
    });

    function canTurn() {
      return playing && visible && !document.hidden;
    }

    function show(index) {
      current = (index + shots.length) % shots.length;
      shots.forEach(function (shot, i) {
        shot.classList.toggle('active', i === current);
      });
      tabs.forEach(function (tab, i) {
        tab.classList.toggle('active', i === current);
        tab.setAttribute('aria-selected', i === current ? 'true' : 'false');
      });

      // the progress bar starts over with each shot
      showcase.classList.remove('playing');
      void showcase.offsetWidth;
      clearTimeout(timer);
      if (canTurn()) {
        showcase.classList.add('playing');
        timer = setTimeout(function () {
          show(current + 1);
        }, kShotSeconds * 1000);
      }

      // on a narrow screen the chips scroll sideways; scrollIntoView would move the page too
      if (tabsBox.scrollWidth > tabsBox.clientWidth) {
        tabsBox.scrollTo({ left: tabs[current].offsetLeft - tabsBox.offsetLeft - 16, behavior: 'smooth' });
      }
    }

    function stop() {
      playing = false;
      clearTimeout(timer);
      showcase.classList.remove('playing');
    }

    if ('IntersectionObserver' in window) {
      new IntersectionObserver(function (entries) {
        visible = entries[0].isIntersecting;
        show(current);
      }, { threshold: 0.4 }).observe(showcase);
    }
    document.addEventListener('visibilitychange', function () {
      show(current);
    });

    // the full-size view

    var boxImage = box.querySelector('img');
    var boxCaption = box.querySelector('.lightbox-caption');
    var boxIndex = 0;

    function openBox(index) {
      boxIndex = (index + shots.length) % shots.length;
      var shot = shots[boxIndex];
      boxImage.src = shot.querySelector('img').src;
      boxCaption.innerHTML = '<b>' + shot.querySelector('figcaption h3').innerHTML + '</b> · '
        + shot.querySelector('figcaption p').innerHTML;
      if (!box.open) {
        box.showModal();
      }
    }

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
    box.addEventListener('close', function () {
      show(boxIndex);
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

    show(0);
  }

  initShowcase();
  load();
})();
