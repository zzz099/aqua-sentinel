const timeTargets = Array.from(document.querySelectorAll("[data-clock-time]"));
const dateTargets = Array.from(document.querySelectorAll("[data-clock-date]"));

function renderClock() {
  const now = new Date();
  const timeText = new Intl.DateTimeFormat("zh-CN", {
    timeZone: "Asia/Shanghai",
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
    hour12: false,
  }).format(now);

  const dateText = new Intl.DateTimeFormat("zh-CN", {
    timeZone: "Asia/Shanghai",
    year: "numeric",
    month: "2-digit",
    day: "2-digit",
    weekday: "long",
  }).format(now);

  for (const target of timeTargets) {
    target.textContent = timeText;
  }

  for (const target of dateTargets) {
    target.textContent = dateText;
  }
}

renderClock();
setInterval(renderClock, 1000);
