effects.windowAdded.connect(function (window) {
    window.animation = set({
        window: window,
        curve: QEasingCurve.Linear,
        duration: animationTime(1000),
        type: Effect.Opacity,
        from: 0,
        to: 1,
        keepAlive: false
    });

    window.minimizedChanged.connect(() => {
        if (!window.minimized) {
            return;
        }
        if (complete(window.animation)) {
            sendTestResponse('ok');
        } else {
            sendTestResponse('fail');
        }
    });
});
