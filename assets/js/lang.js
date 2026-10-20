// The page carries every text in both languages and shows the one <html data-lang> names; the head
// script picks it before the first paint. What scripts write follows the same choice through t().

const root = document.documentElement;
const listeners = [];

export function lang() {
  return root.dataset.lang === 'ru' ? 'ru' : 'en';
}

export function t(ru, en) {
  return lang() === 'ru' ? ru : en;
}

export function onLangChange(fn) {
  listeners.push(fn);
}

export function initLangSwitch() {
  const button = document.getElementById('lang-switch');
  // the button shows the other language's code, which is also its name for a screen reader;
  // the title names that language in it
  const title = () => {
    button.title = t('Switch to English', 'Переключить на русский');
  };

  button.addEventListener('click', () => {
    const value = lang() === 'ru' ? 'en' : 'ru';
    root.dataset.lang = value;
    root.lang = value;
    try {
      localStorage.setItem('lang', value);
    } catch (e) {}
    title();
    listeners.forEach((fn) => fn());
  });
  title();
}
