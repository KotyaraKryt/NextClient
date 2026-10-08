(function () {
  'use strict';

  // the repository whose releases the site offers and lists
  var kRepo = 'CS-NextClient/NextClient';
  var kReleasesUrl = 'https://github.com/' + kRepo + '/releases';
  var kApiUrl = 'https://api.github.com/repos/' + kRepo + '/releases?per_page=50';
  // without a token GitHub answers 60 requests an hour per address, so one answer serves the whole visit
  var kCacheKey = 'releases:' + kRepo;
  var kCacheSeconds = 600;
  var kShownAtFirst = 4;

  var releases = null;
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

  function renderDownload() {
    var button = document.getElementById('download');
    var meta = document.getElementById('download-meta');
    var others = document.getElementById('other-downloads');
    var latest = releases && releases[0];
    if (!latest) {
      return;
    }

    // the visitor's own system first, Windows when there's no build for it
    var assets = pickAssets(latest);
    var mainOs = kOs === 'linux' && assets.linux ? 'linux' : (assets.windows ? 'windows' : (assets.linux ? 'linux' : null));
    var main = mainOs ? assets[mainOs] : null;
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
    renderReleases();
  }

  // a link to a version further down the list opens the whole list
  function revealHash() {
    var id = decodeURIComponent(location.hash.slice(1));
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
        return cached.releases;
      }
    } catch (e) {}
    return null;
  }

  function writeCache(list) {
    try {
      sessionStorage.setItem(kCacheKey, JSON.stringify({ time: Date.now(), releases: list }));
    } catch (e) {}
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
          return { name: asset.name, size: asset.size, browser_download_url: asset.browser_download_url };
        })
      };
    });
  }

  function load() {
    var cached = readCache();
    if (cached) {
      releases = cached;
      render();
      revealHash();
      return;
    }

    fetch(kApiUrl, { headers: { Accept: 'application/vnd.github+json' } })
      .then(function (response) {
        if (!response.ok) {
          throw new Error('HTTP ' + response.status);
        }
        return response.json();
      })
      .then(function (list) {
        releases = trim(list);
        writeCache(releases);
      })
      .catch(function () {
        releases = [];
      })
      .then(function () {
        render();
        revealHash();
      });
  }

  load();
})();
