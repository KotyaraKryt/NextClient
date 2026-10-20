// The little of Markdown that release notes use: headings, lists, paragraphs, bold, code and links.

import { kRepoUrl } from './github.js';

export function escapeHtml(value) {
  return value.replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]);
}

function inline(value) {
  // code goes aside first, so nothing inside it turns into a link or bold text
  const codes = [];
  const html = escapeHtml(value)
    .replace(/`([^`]+)`/g, (_, code) => {
      codes.push(`<code>${code}</code>`);
      return `\u0000${codes.length - 1}\u0000`;
    })
    .replace(/\*\*([^*]+)\*\*/g, '<b>$1</b>')
    .replace(/\[([^\]]+)\]\((https?:\/\/[^\s)]+)\)/g, '<a href="$2" target="_blank" rel="noopener">$1</a>')
    .replace(/(^|[\s(])(https?:\/\/[^\s)<]+)/g, '$1<a href="$2" target="_blank" rel="noopener">$2</a>')
    .replace(/(^|[\s(])#(\d+)\b/g, `$1<a href="${kRepoUrl}/issues/$2" target="_blank" rel="noopener">#$2</a>`);
  return html.replace(/\u0000(\d+)\u0000/g, (_, i) => codes[+i]);
}

export function markdown(source) {
  const out = [];
  let paragraph = [];
  let list = null;

  const flushParagraph = () => {
    if (paragraph.length) {
      out.push(`<p>${inline(paragraph.join(' '))}</p>`);
      paragraph = [];
    }
  };
  const flushList = () => {
    if (list) {
      out.push(`<${list.tag}>${list.items.map((item) => `<li>${inline(item)}</li>`).join('')}</${list.tag}>`);
      list = null;
    }
  };

  for (const line of source.replace(/\r\n?/g, '\n').split('\n')) {
    const heading = /^\s*#{1,6}\s+(.*)$/.exec(line);
    const item = /^\s*(\d+[.)]|[-*+])\s+(.*)$/.exec(line);
    if (!line.trim()) {
      flushParagraph();
    } else if (heading) {
      flushParagraph();
      flushList();
      out.push(`<h4>${inline(heading[1])}</h4>`);
    } else if (item) {
      flushParagraph();
      const tag = /\d/.test(item[1]) ? 'ol' : 'ul';
      if (list && list.tag !== tag && !/^\s{2,}/.test(line)) {
        flushList();
      }
      list = list || { tag, items: [] };
      list.items.push(item[2]);
    } else if (list && /^\s+/.test(line)) {
      // a wrapped line of the item above
      list.items[list.items.length - 1] += ' ' + line.trim();
    } else {
      flushList();
      paragraph.push(line.trim());
    }
  }
  flushParagraph();
  flushList();
  return out.join('');
}

// the notes are written twice, under "## EN" and "## RU"; the other language stands in when one is missing
export function notesFor(body, language) {
  const sections = {};
  let current = null;
  for (const line of (body || '').replace(/\r\n?/g, '\n').split('\n')) {
    const head = /^##\s*(EN|RU)\s*$/i.exec(line.trim());
    if (head) {
      current = head[1].toLowerCase();
      sections[current] = [];
    } else if (current) {
      sections[current].push(line);
    }
  }
  const chosen = sections[language] || sections[language === 'ru' ? 'en' : 'ru'];
  return chosen ? chosen.join('\n').trim() : (body || '').trim();
}
