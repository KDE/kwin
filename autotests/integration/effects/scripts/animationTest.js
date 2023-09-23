effects.windowAdded.connect(function(w) {
    w.anim1 = effect.animate(w, Effect.Scale, 100, 1.4, 0.2, 0, QEasingCurve.OutCubic);
    sendTestResponse(typeof(w.anim1) == "number");

    w.windowUnminimized.connect(() => cancel(w.anim1));
    w.windowMinimized.connect(() => retarget(w.anim1, 1.5, 200));
});
