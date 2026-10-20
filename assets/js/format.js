import { lang, t } from './lang.js';

const locale = () => (lang() === 'ru' ? 'ru-RU' : 'en-US');

export function formatNumber(value) {
  return value.toLocaleString(locale());
}

export function formatSize(bytes) {
  return (bytes / 1048576).toLocaleString(locale(), { minimumFractionDigits: 1, maximumFractionDigits: 1 }) + t(' МБ', ' MB');
}

export function formatDate(iso) {
  try {
    return new Date(iso).toLocaleDateString(locale(), { day: 'numeric', month: 'long', year: 'numeric' });
  } catch (e) {
    return iso.slice(0, 10);
  }
}

// "16 дней назад", "yesterday"
export function formatAgo(iso) {
  const days = Math.max(0, Math.floor((Date.now() - new Date(iso).getTime()) / 86400000));
  try {
    const format = new Intl.RelativeTimeFormat(lang(), { numeric: 'auto' });
    return days < 60 ? format.format(-days, 'day') : format.format(-Math.round(days / 30), 'month');
  } catch (e) {
    return iso.slice(0, 10);
  }
}

// 1 скачивание, 2 скачивания, 5 скачиваний
export function plural(n, one, few, many) {
  const tens = n % 100;
  const units = n % 10;
  if (tens >= 11 && tens <= 14) {
    return many;
  }
  return units === 1 ? one : units >= 2 && units <= 4 ? few : many;
}
