import { initChangelog, renderChangelog, revealVersion } from './changelog.js';
import { initDownload, renderDownload } from './download.js';
import { loadReleases } from './github.js';
import { initLangSwitch, onLangChange } from './lang.js';
import { initFaq, initHeader, initInstall } from './page.js';
import { initTour } from './tour.js';

initLangSwitch();
initHeader();
initInstall();
initFaq();
initTour();
initDownload();
initChangelog();

// what GitHub said about the releases; an empty list when it could not be asked, null until then
let data = null;

function render() {
  if (data) {
    renderDownload(data);
    renderChangelog(data);
  }
}

onLangChange(render);
loadReleases()
  .catch(() => ({ releases: [], downloads: 0 }))
  .then((loaded) => {
    data = loaded;
    render();
    revealVersion();
  });
