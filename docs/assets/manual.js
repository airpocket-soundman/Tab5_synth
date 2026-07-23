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

})();
