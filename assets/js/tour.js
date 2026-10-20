// The tour: one board per step, staying in place while the steps scroll past it, and the same boards
// in a dialog, as large as the screen allows.

import { onLangChange } from './lang.js';
import { onScroll } from './scroll.js';

// a board is laid out at 800x450 and scaled by --k; its notes are drawn outside the scaled view on
// whole pixels, from boxes given in the pixels of the shot they mark, so a 1px line stays a sharp 1px
function drawNotes(board, k, width) {
  let layer = board.querySelector('.board-notes');
  if (!layer) {
    layer = document.createElement('div');
    layer.className = 'board-notes';
    board.append(layer);
  }
  layer.textContent = '';

  const add = (name, left, top, w, h) => {
    const el = document.createElement('i');
    el.className = name;
    el.style.left = `${left}px`;
    el.style.top = `${top}px`;
    if (w !== null) {
      el.style.width = `${w}px`;
    }
    if (h !== null) {
      el.style.height = `${h}px`;
    }
    layer.append(el);
    return el;
  };

  for (const note of board.querySelectorAll('.board-note')) {
    const frame = note.closest('.board-frame');
    if (frame && frame.hidden) {
      continue;
    }
    const shot = (frame || board).querySelector(`[data-shot="${note.dataset.on}"]`);
    const s = shot.offsetWidth / Number(shot.getAttribute('width'));
    const b = note.dataset.box.split(' ').map(Number);
    // a little outside what it marks
    const left = Math.round((shot.offsetLeft + b[0] * s) * k) - 3;
    const top = Math.round((shot.offsetTop + b[1] * s) * k) - 3;
    const right = Math.round((shot.offsetLeft + (b[0] + b[2]) * s) * k) + 3;
    const bottom = Math.round((shot.offsetTop + (b[1] + b[3]) * s) * k) + 3;
    add('note-box', left, top, right - left, bottom - top);

    const tag = add('note-tag', 0, 0, null, null);
    tag.innerHTML = note.innerHTML;
    const w = tag.offsetWidth;
    const h = tag.offsetHeight;
    const at = Math.round(Number(note.dataset.at) * k);
    const midX = Math.round((left + right) / 2);
    const midY = Math.round((top + bottom) / 2);
    let x = left;
    let y = top;
    switch (note.dataset.side) {
      case 'right':
        // the tag stays on the board when the board is narrower than the layout assumed
        x = Math.min(at, width - 8 - w);
        add('note-line', right, midY, x - right, null);
        y = midY - Math.round(h / 2);
        break;
      case 'left':
        x = Math.max(8, at - w);
        add('note-line', x + w, midY, left - x - w, null);
        y = midY - Math.round(h / 2);
        break;
      case 'above':
        add('note-line v', midX, at, null, top - at);
        x = midX - Math.round(w / 2);
        y = at - h;
        break;
      case 'below':
        add('note-line v', midX, bottom, null, at - bottom);
        x = midX - Math.round(w / 2);
        y = at;
        break;
    }
    tag.style.left = `${x}px`;
    tag.style.top = `${y}px`;
  }
}

export function initTour() {
  const section = document.getElementById('tour');
  const stage = document.getElementById('tour-boards');
  const box = document.getElementById('lightbox');
  const steps = [...section.querySelectorAll('.tour-step')];
  const boards = [...stage.querySelectorAll('.board')];
  const progress = document.getElementById('tour-progress');
  const home = stage.parentNode;
  const slot = document.getElementById('lightbox-stage');
  const caption = box.querySelector('.lightbox-caption');
  const placeholder = document.createElement('div');
  placeholder.className = 'tour-placeholder';
  let shown = 0;
  let scrolled = 0;

  const layout = () => {
    const width = stage.clientWidth;
    const k = width / 800;
    stage.style.setProperty('--k', k.toFixed(4));
    stage.classList.toggle('narrow', width < 480);
    boards.forEach((board) => drawNotes(board, k, width));
  };

  const show = (index) => {
    shown = index;
    boards.forEach((board, i) => board.classList.toggle('active', i === index));
    if (box.open) {
      caption.innerHTML = `<b>${steps[index].querySelector('h3').innerHTML}</b> · ${steps[index].querySelector('p').innerHTML}`;
    }
  };

  for (const board of boards) {
    const chips = board.querySelector('.board-chips');
    if (!chips) {
      continue;
    }
    chips.addEventListener('click', (event) => {
      const button = event.target.closest('button');
      if (!button) {
        return;
      }
      for (const other of chips.querySelectorAll('button')) {
        other.setAttribute('aria-pressed', String(other === button));
      }
      for (const frame of board.querySelectorAll('.board-frame')) {
        frame.hidden = frame.dataset.frame !== button.dataset.frame;
      }
      drawNotes(board, stage.clientWidth / 800, stage.clientWidth);
    });
  }

  // the observer's first call lays the boards out once the page has a layout of its own
  new ResizeObserver(layout).observe(stage);
  // a tag's width depends on the font and on the language shown
  document.fonts.ready.then(layout);
  onLangChange(layout);
  for (const img of stage.querySelectorAll('img')) {
    if (!img.complete) {
      img.addEventListener('load', layout);
    }
  }

  // the step that crosses the middle of the screen is the one shown
  const observer = new IntersectionObserver((entries) => {
    for (const entry of entries) {
      if (entry.isIntersecting) {
        scrolled = steps.indexOf(entry.target);
        steps.forEach((step, i) => step.classList.toggle('active', i === scrolled));
        if (!box.open) {
          show(scrolled);
        }
      }
    }
  }, { rootMargin: '-50% 0px -50% 0px' });
  steps.forEach((step) => observer.observe(step));

  const list = section.querySelector('.tour-steps');
  onScroll(() => {
    const rect = list.getBoundingClientRect();
    const p = Math.min(1, Math.max(0, (window.innerHeight / 2 - rect.top) / rect.height));
    progress.style.setProperty('--p', p.toFixed(4));
  });

  // the dialog borrows the stage, and its arrows walk the steps
  const step = (delta) => show((shown + delta + steps.length) % steps.length);

  document.getElementById('tour-expand').addEventListener('click', () => {
    home.insertBefore(placeholder, stage);
    slot.append(stage);
    box.showModal();
    show(shown);
  });
  box.addEventListener('close', () => {
    home.insertBefore(stage, placeholder);
    placeholder.remove();
    show(scrolled);
  });
  box.querySelector('.lightbox-close').addEventListener('click', () => box.close());
  box.querySelector('.lightbox-prev').addEventListener('click', () => step(-1));
  box.querySelector('.lightbox-next').addEventListener('click', () => step(1));
  box.addEventListener('click', (event) => {
    if (event.target === box) {
      box.close();
    }
  });
  box.addEventListener('keydown', (event) => {
    if (event.key === 'ArrowLeft') {
      step(-1);
    } else if (event.key === 'ArrowRight') {
      step(1);
    }
  });

  let touchX = null;
  box.addEventListener('touchstart', (event) => {
    touchX = event.target.closest('.board-chips') ? null : event.touches[0].clientX;
  }, { passive: true });
  box.addEventListener('touchend', (event) => {
    if (touchX === null) {
      return;
    }
    const dx = event.changedTouches[0].clientX - touchX;
    touchX = null;
    if (Math.abs(dx) > 50) {
      step(dx < 0 ? 1 : -1);
    }
  });

  show(0);
}
