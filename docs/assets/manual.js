(() => {
  const frame = document.getElementById("capture-simulator");
  const shots = [...document.querySelectorAll("[data-sim-page]")];
  if (!frame || shots.length === 0) return;
  shots.forEach(image => {
    if (image.getAttribute("src")) image.parentElement.querySelector(".sim-fallback")?.remove();
  });

  const pageX = { amp: 86, fx: 203, lfo: 320, bank: 437 };
  const pageY = 155;

  const waitForCanvas = () => new Promise((resolve, reject) => {
    const started = Date.now();
    const poll = () => {
      try {
        const canvas = frame.contentDocument?.getElementById("canvas");
        if (canvas && canvas.width === 1280 && canvas.height === 720 &&
            getComputedStyle(canvas).visibility !== "hidden") {
          resolve(canvas);
          return;
        }
      } catch (_) {
        // The simulator is expected to be same-origin on GitHub Pages.
      }
      if (Date.now() - started > 15000) reject(new Error("Simulator timeout"));
      else setTimeout(poll, 100);
    };
    poll();
  });

  const pointer = (canvas, type, x, y) => {
    const rect = canvas.getBoundingClientRect();
    const clientX = rect.left + x * rect.width / canvas.width;
    const clientY = rect.top + y * rect.height / canvas.height;
    canvas.dispatchEvent(new PointerEvent(type, {
      bubbles: true,
      cancelable: true,
      pointerId: 1,
      pointerType: "mouse",
      isPrimary: true,
      buttons: type === "pointerup" ? 0 : 1,
      clientX,
      clientY
    }));
  };

  frame.addEventListener("load", async () => {
    try {
      const canvas = await waitForCanvas();
      for (const image of shots) {
        const page = image.dataset.simPage;
        pointer(canvas, "pointerdown", pageX[page], pageY);
        await new Promise(resolve => setTimeout(resolve, 70));
        pointer(canvas, "pointerup", pageX[page], pageY);
        await new Promise(resolve => setTimeout(resolve, 220));
        image.src = canvas.toDataURL("image/png");
        image.hidden = false;
        image.parentElement.querySelector(".sim-fallback")?.remove();
        const status = image.closest(".sim-figure")?.querySelector(".capture-status");
        if (status) status.textContent = status.dataset.ready;
      }
    } catch (error) {
      document.querySelectorAll(".capture-status").forEach(status => {
        status.textContent = status.dataset.failed;
      });
    }
  }, { once: true });
})();
