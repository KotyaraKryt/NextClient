// The download buttons, the other builds under them, the stats line and the archive's SHA-256.

import { t } from './lang.js';
import { formatAgo, formatNumber, formatSize, plural } from './format.js';
import { stableOf, versionOf } from './github.js';
import { escapeHtml } from './markdown.js';
import { kOs, kOsNames } from './platform.js';

// the game's archive, its Linux one when there is one; never the debug symbols
function pickAssets(release) {
  const assets = (release.assets || []).filter((asset) => !/pdb/i.test(asset.name));
  return {
    windows: assets.find((asset) => /\.zip$/i.test(asset.name)),
    linux: assets.find((asset) => /\.tar\.gz$/i.test(asset.name)),
  };
}

// the visitor's own system first, Windows when there's no build for it
function offer(release) {
  const assets = pickAssets(release);
  const os = kOs === 'linux' && assets.linux ? 'linux' : assets.windows ? 'windows' : assets.linux ? 'linux' : null;
  return { assets, os, asset: os ? assets[os] : null };
}

function assetLink(asset, os) {
  return `<a href="${asset.browser_download_url}">${t('сборка для ', 'the ')}${kOsNames[os]}${t('', ' build')}</a>`
    + ` (${formatSize(asset.size)})`;
}

function renderButtons(latest, choice) {
  for (const button of document.querySelectorAll('[data-download]')) {
    button.href = choice.asset ? choice.asset.browser_download_url : latest.html_url;
  }
  for (const meta of document.querySelectorAll('[data-download-meta]')) {
    meta.textContent = `v${versionOf(latest)}`
      + (latest.prerelease ? ` · ${t('превью', 'preview')}` : '')
      + (choice.asset ? ` · ${formatSize(choice.asset.size)} · ${kOsNames[choice.os]}` : '');
  }
}

function renderOthers(releases, latest, choice) {
  const notes = [];
  if (kOs === 'linux' && !choice.assets.linux) {
    notes.push(t('Для этой версии сборки под Linux нет.', 'This version has no Linux build.'));
  } else if (kOs === 'mac') {
    notes.push(t('Для macOS сборки нет.', 'There is no macOS build.'));
  }
  const otherOs = choice.os === 'windows' ? 'linux' : 'windows';
  if (choice.asset && choice.assets[otherOs]) {
    notes.push(`${t('Есть и ', 'There is also ')}${assetLink(choice.assets[otherOs], otherOs)}.`);
  }
  const stable = latest.prerelease ? stableOf(releases) : null;
  const stableChoice = stable ? offer(stable) : null;
  if (stableChoice && stableChoice.asset) {
    notes.push(`${t('Стабильная версия ', 'The stable version ')}${escapeHtml(versionOf(stable))}: `
      + `${assetLink(stableChoice.asset, stableChoice.os)}.`);
  }
  const box = document.getElementById('other-downloads');
  box.innerHTML = notes.join(' ');
  box.hidden = !notes.length;
}

// what others already did, and that the project is alive
function renderStats(latest, downloads) {
  const parts = [];
  if (downloads) {
    const word = t(plural(downloads, 'скачивание', 'скачивания', 'скачиваний'), downloads === 1 ? 'download' : 'downloads');
    parts.push(`<span><b>${formatNumber(downloads)}</b> ${word}</span>`);
  }
  parts.push(`<span>${t('обновлено ', 'updated ')}<b>${formatAgo(latest.published_at)}</b></span>`);
  const box = document.getElementById('proof-stats');
  box.innerHTML = parts.join('');
  box.hidden = false;
}

// the main button's archive, to check against what was downloaded
function renderChecksum(choice) {
  const box = document.getElementById('checksum');
  const digest = choice.asset ? choice.asset.digest || '' : '';
  const hash = /^sha256:[0-9a-f]{64}$/.test(digest) ? digest.slice(7) : '';
  box.hidden = !hash;
  if (!hash) {
    return;
  }
  const button = document.getElementById('checksum-copy');
  document.getElementById('checksum-file').textContent = choice.asset.name;
  button.textContent = `${hash.slice(0, 10)}…${hash.slice(-10)}`;
  button.title = hash;
  button.dataset.hash = hash;
  button.setAttribute('aria-label', t('Скопировать SHA-256', 'Copy SHA-256'));
}

export function renderDownload({ releases, downloads }) {
  const latest = releases[0];
  if (!latest) {
    return;
  }
  const choice = offer(latest);
  renderButtons(latest, choice);
  renderOthers(releases, latest, choice);
  renderStats(latest, downloads);
  renderChecksum(choice);
}

export function initDownload() {
  const button = document.getElementById('checksum-copy');
  button.addEventListener('click', async () => {
    if (!navigator.clipboard || !button.dataset.hash) {
      return;
    }
    await navigator.clipboard.writeText(button.dataset.hash);
    const shown = button.textContent;
    button.textContent = t('Скопировано', 'Copied');
    button.classList.add('copied');
    setTimeout(() => {
      button.textContent = shown;
      button.classList.remove('copied');
    }, 1400);
  });
}
