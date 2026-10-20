// The header, the install tabs and links to questions.

import { kOs } from './platform.js';
import { onScroll } from './scroll.js';

// the header's line once the page has scrolled, its own download button once the hero's is gone,
// and the link of the section in view
export function initHeader() {
  const root = document.documentElement;
  onScroll((scroll) => root.classList.toggle('scrolled', scroll > 40));

  new IntersectionObserver(([entry]) => {
    root.classList.toggle('cta-away', !entry.isIntersecting);
  }).observe(document.querySelector('.hero [data-download]'));

  const links = [...document.querySelectorAll('.links a')];
  const sections = new IntersectionObserver((entries) => {
    for (const entry of entries) {
      if (entry.isIntersecting) {
        links.forEach((link) => link.classList.toggle('current', link.hash === `#${entry.target.id}`));
      }
    }
  }, { rootMargin: '-45% 0px -50% 0px' });
  links.forEach((link) => sections.observe(document.querySelector(link.hash)));
}

// the install steps open on the visitor's own system
export function initInstall() {
  const tabs = [...document.querySelectorAll('.os-tabs [data-os]')];
  const select = (os) => {
    for (const tab of tabs) {
      const on = tab.dataset.os === os;
      tab.setAttribute('aria-selected', String(on));
      document.getElementById(tab.getAttribute('aria-controls')).hidden = !on;
    }
  };
  tabs.forEach((tab) => tab.addEventListener('click', () => select(tab.dataset.os)));
  select(kOs === 'linux' ? 'linux' : 'windows');
}

// a link to a question opens it
export function initFaq() {
  const open = () => {
    const question = /^#faq-[\w-]+$/.test(location.hash) ? document.querySelector(location.hash) : null;
    if (question && question.tagName === 'DETAILS') {
      question.open = true;
      question.scrollIntoView({ block: 'start' });
    }
  };
  window.addEventListener('hashchange', open);
  open();
}
