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

    window.windowMinimized.connect(() => {
        if (redirect(window.animation, Effect.Backward, Effect.DontTerminate)) {
            sendTestResponse('ok');
        } else {
            sendTestResponse('fail');
        }
    });
});
