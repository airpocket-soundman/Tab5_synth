(() => {
  document.querySelectorAll(".env-canvas").forEach(canvas => {
    const context = canvas.getContext("2d");
    const points = [[24, 160], [230, 18], [430, 84], [720, 84], [976, 160]];
    const colors = ["#e35b4f", "#e3a93d", "#a9c74d", "#42d4d1"];

    context.lineCap = "round";
    context.lineJoin = "round";
    context.lineWidth = 9;
    context.strokeStyle = "#07110f";
    context.beginPath();
    context.moveTo(points[0][0], points[0][1]);
    points.slice(1).forEach(point => context.lineTo(point[0], point[1]));
    context.stroke();

    context.lineWidth = 5;
    colors.forEach((color, index) => {
      context.strokeStyle = color;
      context.beginPath();
      context.moveTo(points[index][0], points[index][1]);
      context.lineTo(points[index + 1][0], points[index + 1][1]);
      context.stroke();
    });
  });

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
