// 'windows', 'linux', 'mac' or 'other'; Android says Linux but gets no build
function detect() {
  const platform = (navigator.userAgentData && navigator.userAgentData.platform) || navigator.platform || '';
  const agent = navigator.userAgent || '';
  if (/android/i.test(agent)) {
    return 'other';
  }
  if (/mac|iphone|ipad/i.test(platform) || /mac os x/i.test(agent)) {
    return 'mac';
  }
  if (/linux|x11|cros/i.test(platform) || /linux|cros/i.test(agent)) {
    return 'linux';
  }
  return 'windows';
}

export const kOs = detect();
export const kOsNames = { windows: 'Windows', linux: 'Linux' };
