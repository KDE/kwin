effects.windowAdded.connect(function (window) {
    window.animation = set({
        window: window,
        curve: QEasingCurve.Linear,
        duration: animationTime(1000),
        type: Effect.Opacity,
        from: 0.0,
        to: 1.0,
        keepAlive: false
    });
});

effects.windowMinimized.connect(function (window) {
    if (redirect(window.animation, Effect.Backward, Effect.TerminateAtSource)) {
        sendTestResponse('ok');
    } else {
        sendTestResponse('fail');
    }
});
