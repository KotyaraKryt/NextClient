// The changelog: every release's notes in the page's language, the newest first.

import { lang, t } from './lang.js';
import { formatDate } from './format.js';
import { kRepoUrl, stableOf, versionOf } from './github.js';
import { escapeHtml, markdown, notesFor } from './markdown.js';

const kShownAtFirst = 2;

let releases = null;
let showAll = false;

function article(release, index, stable) {
  const version = escapeHtml(versionOf(release));
  const title = release.name && !release.name.includes(versionOf(release)) ? release.name : '';
  const badge = release.prerelease
    ? `<span class="badge preview">${t('превью', 'preview')}</span>`
    : release === stable ? `<span class="badge">${t('стабильная', 'stable')}</span>` : '';
  const link = `<a href="${release.html_url}" target="_blank" rel="noopener">`
    + `${t('Релиз', 'Release')}<span class="sr-only"> ${version}</span> ${t('на GitHub', 'on GitHub')} →</a>`;
  return `<article class="release${index === 0 ? ' latest' : ''}" id="v${version}">
    <div class="release-head">
      <h3 class="release-version"><a href="#v${version}">${version}</a></h3>
      ${badge}
      <span class="release-date">${formatDate(release.published_at)}</span>
      ${title ? `<span class="release-title">${escapeHtml(title)}</span>` : ''}
    </div>
    <div class="release-body">${markdown(notesFor(release.body, lang()))}</div>
    <div class="release-foot">${link}</div>
  </article>`;
}

// draws the list it was last given when called without one
export function renderChangelog(data) {
  releases = data ? data.releases : releases;
  if (!releases) {
    return;
  }
  const container = document.getElementById('releases');
  const more = document.getElementById('more');
  if (!releases.length) {
    container.innerHTML = `<p class="releases-error">${t('Не удалось загрузить список изменений. Он есть на ', 'Could not load the changelog. It is on ')}`
      + `<a href="${kRepoUrl}/releases" target="_blank" rel="noopener">GitHub</a>.</p>`;
    more.hidden = true;
    return;
  }
  const stable = stableOf(releases);
  const shown = showAll ? releases : releases.slice(0, kShownAtFirst);
  container.innerHTML = shown.map((release, index) => article(release, index, stable)).join('');
  more.hidden = showAll || releases.length <= kShownAtFirst;
}

// a link to a version further down the list opens the whole list
export function revealVersion() {
  const id = decodeURIComponent(location.hash.slice(1));
  if (!/^v[\w.-]+$/.test(id) || !releases) {
    return;
  }
  if (!document.getElementById(id) && releases.some((release) => `v${versionOf(release)}` === id)) {
    showAll = true;
    renderChangelog();
  }
  const target = document.getElementById(id);
  if (target) {
    target.scrollIntoView({ block: 'start' });
    target.classList.remove('highlight');
    // a fresh layout restarts the highlight when the same version is opened again
    void target.offsetWidth;
    target.classList.add('highlight');
  }
}

export function initChangelog() {
  document.getElementById('more').addEventListener('click', () => {
    showAll = true;
    renderChangelog();
  });
  window.addEventListener('hashchange', revealVersion);
}
