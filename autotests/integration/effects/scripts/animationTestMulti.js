effects.windowAdded.connect(function(w) {
    w.anim1 = animate({
        window: w,
        duration: 100,
        animations: [{
            type: Effect.Scale,
            to: 1.4,
            curve: QEasingCurve.OutCubic
        }, {
            type: Effect.Opacity,
            curve: QEasingCurve.OutCubic,
            to: 0.0
        }]
    });
    sendTestResponse(typeof(w.anim1) == "object" && Array.isArray(w.anim1));
});

effects.windowUnminimized.connect(function(w) {
    cancel(w.anim1);
});

effects.windowMinimized.connect(function(w) {
    retarget(w.anim1, 1.5, 200);
});
