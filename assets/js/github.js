// The releases the site offers and lists, straight from GitHub's API.

export const kRepo = 'CS-NextClient/NextClient';
export const kRepoUrl = `https://github.com/${kRepo}`;
const kReleasesApi = `https://api.github.com/repos/${kRepo}/releases?per_page=100`;
// without a token GitHub answers 60 requests an hour per address, so one answer serves the whole visit
const kCacheKey = `site:4:${kRepo}`;
const kCacheMs = 10 * 60 * 1000;

// only what the page shows is kept, so the cache stays small; a preview stays only while it is newer
// than the last stable release, so the previews drop out once their version ships
function trim(all) {
  let stableSeen = false;
  return all
    .filter((release) => {
      if (release.draft || (release.prerelease && stableSeen)) {
        return false;
      }
      stableSeen = stableSeen || !release.prerelease;
      return true;
    })
    .map((release) => ({
      tag_name: release.tag_name,
      prerelease: release.prerelease,
      name: release.name,
      body: release.body,
      html_url: release.html_url,
      published_at: release.published_at,
      assets: (release.assets || []).map((asset) => ({
        name: asset.name,
        size: asset.size,
        digest: asset.digest,
        browser_download_url: asset.browser_download_url,
      })),
    }));
}

// every archive of every release, test builds included; the debug symbols aren't the game
function countDownloads(all) {
  return all.reduce((sum, release) => sum + (release.assets || [])
    .reduce((n, asset) => n + (/pdb/i.test(asset.name) ? 0 : asset.download_count || 0), 0), 0);
}

function readCache() {
  try {
    const cached = JSON.parse(sessionStorage.getItem(kCacheKey));
    if (cached && Date.now() - cached.time < kCacheMs) {
      return cached.data;
    }
  } catch (e) {}
  return null;
}

function writeCache(data) {
  try {
    sessionStorage.setItem(kCacheKey, JSON.stringify({ time: Date.now(), data }));
  } catch (e) {}
}

// { releases, downloads }, the newest release first; rejects when GitHub doesn't answer
export async function loadReleases() {
  const cached = readCache();
  if (cached) {
    return cached;
  }
  const response = await fetch(kReleasesApi, { headers: { Accept: 'application/vnd.github+json' } });
  if (!response.ok) {
    throw new Error(`GitHub answered ${response.status}`);
  }
  const all = await response.json();
  const data = { releases: trim(all), downloads: countDownloads(all) };
  writeCache(data);
  return data;
}

export function versionOf(release) {
  return (release.tag_name || '').replace(/^v/i, '');
}

export function stableOf(releases) {
  return releases.find((release) => !release.prerelease);
}
