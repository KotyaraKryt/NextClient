# NextClient site

A static page served by GitHub Pages straight from this branch, with no dependencies and nothing to
install.

| Path | What it is |
|---|---|
| `src/page.html`, `src/parts/` | the page and its sections, with `{{ key }}` where a text goes |
| `src/strings.toml` | every text of the page, in Russian and in English side by side |
| `index.html` | built from `src/` by `tools/build.py`; not edited by hand |
| `assets/style.css` | the styles |
| `assets/js/` | ES modules, loaded by the browser as they are; `main.js` starts them |
| `assets/game/` | the tour's pictures, cut from screenshots by `tools/game-assets.py` |
| `assets/og.jpg`, `assets/apple-touch-icon.png` | made by `tools/make-images.py` |

To change a text, edit `src/strings.toml` and run `python3 tools/build.py`, then commit `index.html`
with it; `python3 tools/build.py --check` says whether `index.html` is up to date. To look at the page,
run `python3 -m http.server` here and open http://localhost:8000.

The changelog and the download buttons come from the GitHub API at run time, so a new release shows
up without touching the site.
