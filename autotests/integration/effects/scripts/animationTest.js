effects.windowAdded.connect(function(w) {
    w.anim1 = effect.animate(w, Effect.Scale, 100, 1.4, 0.2, 0, QEasingCurve.OutCubic);
    sendTestResponse(typeof(w.anim1) == "number");
});

effects.windowUnminimized.connect(function(w) {
    cancel(w.anim1);
});

effects.windowMinimized.connect(function(w) {
    retarget(w.anim1, 1.5, 200);
});
