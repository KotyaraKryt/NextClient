// Everything that follows the scroll runs in one animation frame, however many scroll events came.

const handlers = [];
let frame = 0;

function run() {
  frame = 0;
  handlers.forEach((fn) => fn(window.scrollY));
}

function request() {
  if (!frame) {
    frame = requestAnimationFrame(run);
  }
}

window.addEventListener('scroll', request, { passive: true });
window.addEventListener('resize', request);

// the handler also runs on the next frame, so it starts from the page's current state; it reads the
// layout, so its first run waits for the frame rather than forcing a layout while the script starts
export function onScroll(fn) {
  handlers.push(fn);
  request();
}
